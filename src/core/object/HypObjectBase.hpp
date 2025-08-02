/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/ObjId.hpp>
#include <core/object/ObjectPool.hpp>
#include <core/object/managed/ManagedObjectResource.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/GlobalContext.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/functional/Delegate.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;
struct HypData;

template <class T>
class HypClassInstance;

template <class T>
struct HypClassDefinition;

enum class HypClassFlags : uint32;

template <class T>
struct Handle;

template <class T>
struct WeakHandle;

namespace dotnet {
class Class;
} // namespace dotnet

extern HYP_API const HypClass* GetClass(TypeId typeId);

extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const HypClass* instanceHypClass);

class HYP_API DynamicHypObjectInitializer final : public IHypObjectInitializer
{
public:
    DynamicHypObjectInitializer(const HypClass* hypClass, IHypObjectInitializer* parentInitializer);
    virtual ~DynamicHypObjectInitializer() override;

    virtual TypeId GetTypeId() const override;

    virtual const HypClass* GetClass() const override
    {
        return m_hypClass;
    }

    virtual void IncRef(void* _this, bool weak) const override
    {
        m_parentInitializer->IncRef(_this, weak);
    }

    virtual void DecRef(void* _this, bool weak) const override
    {
        m_parentInitializer->DecRef(_this, weak);
    }

    virtual uint32 GetRefCount_Strong(void* _this) const override
    {
        return m_parentInitializer->GetRefCount_Strong(_this);
    }

    virtual uint32 GetRefCount_Weak(void* _this) const override
    {
        return m_parentInitializer->GetRefCount_Weak(_this);
    }

private:
    const HypClass* m_hypClass;
    IHypObjectInitializer* m_parentInitializer;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
public:
    HypObjectInitializer(T* _this)
    {
    }

    virtual ~HypObjectInitializer() override = default;

    virtual TypeId GetTypeId() const override
    {
        return GetTypeId_Static();
    }

    virtual const HypClass* GetClass() const override
    {
        return GetClass_Static();
    }

    virtual void IncRef(void* _this, bool weak) const override
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            if (weak)
            {
                static_cast<T*>(_this)->GetObjectHeader_Internal()->IncRefWeak();
            }
            else
            {
                static_cast<T*>(_this)->GetObjectHeader_Internal()->IncRefStrong();
            }
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            auto* refCountData = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weakThis.GetRefCountData_Internal();
            refCountData->incRefCount(_this, *refCountData, weak);
        }
        else
        {
            HYP_FAIL("Unhandled HypClass allocation method");
        }
    }

    virtual void DecRef(void* _this, bool weak) const override
    {
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            if (weak)
            {
                static_cast<T*>(_this)->GetObjectHeader_Internal()->DecRefWeak();
            }
            else
            {
                static_cast<T*>(_this)->GetObjectHeader_Internal()->DecRefStrong();
            }
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            auto* refCountData = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weakThis.GetRefCountData_Internal();

            if (weak)
            {
                refCountData->DecRefCount_Weak(_this);
            }
            else
            {
                refCountData->DecRefCount_Strong(_this);
            }
        }
        else
        {
            HYP_FAIL("Unhandled HypClass allocation method");
        }
    }

    virtual uint32 GetRefCount_Strong(void* _this) const override
    {
        if (!_this)
        {
            return 0;
        }

        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            return static_cast<T*>(_this)->GetObjectHeader_Internal()->GetRefCountStrong();
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            auto* refCountData = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weakThis.GetRefCountData_Internal();
            return refCountData->UseCount_Strong();
        }
        else
        {
            HYP_FAIL("Unhandled HypClass allocation method");
        }

        return 0;
    }

    virtual uint32 GetRefCount_Weak(void* _this) const override
    {
        if (!_this)
        {
            return 0;
        }
        if constexpr (std::is_base_of_v<HypObjectBase, T>)
        {
            return static_cast<T*>(_this)->GetObjectHeader_Internal()->GetRefCountWeak();
        }
        else if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<>, T>)
        {
            auto* refCountData = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weakThis.GetRefCountData_Internal();
            return refCountData->UseCount_Weak();
        }
        else
        {
            HYP_FAIL("Unhandled HypClass allocation method");
        }

        return 0;
    }

    HYP_FORCE_INLINE static TypeId GetTypeId_Static()
    {
        static constexpr TypeId typeId = TypeId::ForType<T>();

        return typeId;
    }

    HYP_FORCE_INLINE static const HypClass* GetClass_Static()
    {
        static const HypClass* hypClass = ::hyperion::GetClass(TypeId::ForType<T>());

        return hypClass;
    }

private:
};

class HYP_API HypObjectBase
{
    template <class T>
    friend struct Handle;

    template <class T>
    friend struct WeakHandle;

    friend struct AnyHandle;

    template <class T, class... Args>
    friend Handle<T> CreateObject(Args&&...);

    template <class T>
    friend bool InitObject(const Handle<T>&);

public:
    virtual ~HypObjectBase();

    HYP_FORCE_INLINE ObjIdBase Id() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");

        return ObjIdBase { m_header->container->GetObjectTypeId(), m_header->index + 1 };
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");

        return m_header->container->GetObjectTypeId();
    }

    HYP_FORCE_INLINE const HypClass* InstanceClass() const
    {
        HYP_CORE_ASSERT(m_header, "Invalid HypObject!");
        HYP_CORE_ASSERT(m_header->container->GetHypClass(), "No HypClass defined for type");

        return m_header->container->GetHypClass();
    }

    HYP_FORCE_INLINE HypObjectHeader* GetObjectHeader_Internal() const
    {
        return m_header;
    }

    void SetManagedObjectResource(ManagedObjectResource* managedObjectResource)
    {
        HYP_CORE_ASSERT(m_managedObjectResource == nullptr);

        m_managedObjectResource = managedObjectResource;
    }

    ManagedObjectResource* GetManagedObjectResource() const
    {
        return m_managedObjectResource;
    }

    dotnet::Object* GetManagedObject() const
    {
        return m_managedObjectResource ? m_managedObjectResource->GetManagedObject() : nullptr;
    }

    HYP_FORCE_INLINE bool IsInitCalled() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_initState.Get(MemoryOrder::RELAXED) & INIT_STATE_READY;
    }

protected:
    enum InitState : uint16
    {
        INIT_STATE_UNINITIALIZED = 0x0,
        INIT_STATE_INIT_CALLED = 0x1,
        INIT_STATE_READY = 0x2
    };

    HypObjectBase();

    HypObjectBase(const HypObjectBase& other) = delete;
    HypObjectBase& operator=(const HypObjectBase& other) = delete;

    HypObjectBase(HypObjectBase&& other) noexcept
        : m_initState(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE)),
          m_delegateHandlers(std::move(other.m_delegateHandlers))
    {
    }

    HypObjectBase& operator=(HypObjectBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_initState.Set(other.m_initState.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
        m_delegateHandlers = std::move(other.m_delegateHandlers);

        return *this;
    }

    /*! \brief Don't call manually (except for instances of derived types calling base class Init() in their own Init() method). */
    virtual void Init()
    {
        // Do nothing by default.
    }

    void SetReady(bool isReady)
    {
        if (isReady)
        {
            m_initState.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        }
        else
        {
            m_initState.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        HYP_CORE_ASSERT(IsReady(), "Object is not in ready state! Was InitObject() called for it?");
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        HYP_CORE_ASSERT(IsInitCalled(), "Object has not had Init() called on it!");
    }

    void AddDelegateHandler(Name name, DelegateHandler&& delegateHandler)
    {
        m_delegateHandlers.Add(name, std::move(delegateHandler));
    }

    void AddDelegateHandler(DelegateHandler&& delegateHandler)
    {
        m_delegateHandlers.Add(std::move(delegateHandler));
    }

    bool RemoveDelegateHandler(WeakName name)
    {
        return m_delegateHandlers.Remove(name);
    }

    // Pointer to the header of the object, holding container, index and ref counts. Must be the first member.
    HypObjectHeader* m_header;
    DelegateHandlerSet m_delegateHandlers;
    ManagedObjectResource* m_managedObjectResource;

private:
    // Used internally by InitObject() to call derived Init() methods.
    void Init_Internal()
    {
        Init();
    }

    AtomicVar<uint16> m_initState;
};

} // namespace hyperion
