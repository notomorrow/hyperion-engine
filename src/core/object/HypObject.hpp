/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_OBJECT_HPP
#define HYPERION_CORE_HYP_OBJECT_HPP

#include <core/ID.hpp>
#include <core/Name.hpp>
#include <core/Defines.hpp>
#include <core/ObjectPool.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypObjectEnums.hpp>
#include <core/object/managed/ManagedObjectResource.hpp>

#include <core/utilities/TypeID.hpp>
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

extern HYP_API const HypClass* GetClass(TypeID type_id);

extern HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const void* ptr, TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass* hyp_class, const HypClass* instance_hyp_class);

template <class ExpectedType>
bool IsInstanceOfHypClass(const HypClass* instance_hyp_class)
{
    if (!instance_hyp_class)
    {
        return false;
    }

    const HypClass* hyp_class = GetClass(TypeID::ForType<ExpectedType>());

    if (!hyp_class)
    {
        return false;
    }

    return IsInstanceOfHypClass(hyp_class, instance_hyp_class);
}

template <class ExpectedType, class InstanceType>
bool IsInstanceOfHypClass(const InstanceType* instance)
{
    static_assert(
        std::is_convertible_v<ExpectedType*, InstanceType*>,
        "IsInstanceOfHypClass() check will never return true as the compared types are fundamentally different.");

    if (!instance)
    {
        return false;
    }

    const HypClass* instance_hyp_class = GetClass(TypeID::ForType<InstanceType>());

    if (!instance_hyp_class)
    {
        return false;
    }

    const HypClass* hyp_class = GetClass(TypeID::ForType<ExpectedType>());

    if (!hyp_class)
    {
        return false;
    }

    // Short-circuit with compile time checking
    if constexpr (std::is_same_v<ExpectedType, InstanceType> || std::is_base_of_v<ExpectedType, InstanceType>)
    {
        return true;
    }

    return IsInstanceOfHypClass(hyp_class, instance_hyp_class);
}

template <class ExpectedType, class InstanceType, typename = std::enable_if_t<!std::is_pointer_v<InstanceType>>>
bool IsInstanceOfHypClass(const InstanceType& instance)
{
    return IsInstanceOfHypClass<ExpectedType, InstanceType>(&instance);
}

class HYP_API DynamicHypObjectInitializer final : public IHypObjectInitializer
{
public:
    DynamicHypObjectInitializer(const HypClass* hyp_class, IHypObjectInitializer* parent_initializer);
    virtual ~DynamicHypObjectInitializer() override;

    virtual TypeID GetTypeID() const override;

    virtual const HypClass* GetClass() const override
    {
        return m_hyp_class;
    }

    virtual void SetManagedObjectResource(ManagedObjectResource* managed_object_resource) override
    {
        m_parent_initializer->SetManagedObjectResource(managed_object_resource);
    }

    virtual ManagedObjectResource* GetManagedObjectResource() const override
    {
        return m_parent_initializer->GetManagedObjectResource();
    }

    virtual dotnet::Object* GetManagedObject() const override
    {
        return m_parent_initializer->GetManagedObject();
    }

    virtual void IncRef(void* _this, bool weak) const override
    {
        m_parent_initializer->IncRef(_this, weak);
    }

    virtual void DecRef(void* _this, bool weak) const override
    {
        m_parent_initializer->DecRef(_this, weak);
    }

    virtual uint32 GetRefCount_Strong(void* _this) const override
    {
        return m_parent_initializer->GetRefCount_Strong(_this);
    }

    virtual uint32 GetRefCount_Weak(void* _this) const override
    {
        return m_parent_initializer->GetRefCount_Weak(_this);
    }

private:
    const HypClass* m_hyp_class;
    IHypObjectInitializer* m_parent_initializer;
};

template <class T>
class HypObjectInitializer final : public IHypObjectInitializer
{
public:
    HypObjectInitializer(T* _this)
        : m_managed_object_resource(nullptr)
    {
    }

    virtual ~HypObjectInitializer() override
    {
        if (m_managed_object_resource)
        {
            FreeResource(m_managed_object_resource);
            m_managed_object_resource = nullptr;
        }
    }

    virtual TypeID GetTypeID() const override
    {
        return GetTypeID_Static();
    }

    virtual const HypClass* GetClass() const override
    {
        return GetClass_Static();
    }

    virtual void SetManagedObjectResource(ManagedObjectResource* managed_object_resource) override
    {
        AssertThrow(m_managed_object_resource == nullptr);
        m_managed_object_resource = managed_object_resource;
    }

    virtual ManagedObjectResource* GetManagedObjectResource() const override
    {
        return m_managed_object_resource;
    }

    virtual dotnet::Object* GetManagedObject() const override
    {
        return m_managed_object_resource ? m_managed_object_resource->GetManagedObject() : nullptr;
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
            auto* ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weak_this.GetRefCountData_Internal();
            ref_count_data->inc_ref_count(_this, *ref_count_data, weak);
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
            auto* ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weak_this.GetRefCountData_Internal();

            if (weak)
            {
                ref_count_data->DecRefCount_Weak(_this);
            }
            else
            {
                ref_count_data->DecRefCount_Strong(_this);
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
            auto* ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weak_this.GetRefCountData_Internal();
            return ref_count_data->UseCount_Strong();
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
            auto* ref_count_data = static_cast<EnableRefCountedPtrFromThisBase<>*>(static_cast<T*>(_this))->weak_this.GetRefCountData_Internal();
            return ref_count_data->UseCount_Weak();
        }
        else
        {
            HYP_FAIL("Unhandled HypClass allocation method");
        }

        return 0;
    }

    HYP_FORCE_INLINE static TypeID GetTypeID_Static()
    {
        static constexpr TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    HYP_FORCE_INLINE static const HypClass* GetClass_Static()
    {
        static const HypClass* hyp_class = ::hyperion::GetClass(TypeID::ForType<T>());

        return hyp_class;
    }

private:
    ManagedObjectResource* m_managed_object_resource;
};

#define HYP_OBJECT_BODY(T, ...)                                                      \
private:                                                                             \
    friend class HypObjectInitializer<T>;                                            \
    friend struct HypClassInitializer_##T;                                           \
    friend class HypClassInstance<T>;                                                \
    friend struct HypClassRegistration<T>;                                           \
                                                                                     \
    HypObjectInitializer<T> m_hyp_object_initializer { this };                       \
    IHypObjectInitializer* m_hyp_object_initializer_ptr = &m_hyp_object_initializer; \
                                                                                     \
public:                                                                              \
    struct HypObjectData                                                             \
    {                                                                                \
        using Type = T;                                                              \
                                                                                     \
        static constexpr bool is_hyp_object = true;                                  \
    };                                                                               \
                                                                                     \
    HYP_FORCE_INLINE IHypObjectInitializer* GetObjectInitializer() const             \
    {                                                                                \
        return m_hyp_object_initializer_ptr;                                         \
    }                                                                                \
                                                                                     \
    HYP_FORCE_INLINE ManagedObjectResource* GetManagedObjectResource() const         \
    {                                                                                \
        return m_hyp_object_initializer_ptr->GetManagedObjectResource();             \
    }                                                                                \
                                                                                     \
    HYP_FORCE_INLINE const HypClass* InstanceClass() const                           \
    {                                                                                \
        return m_hyp_object_initializer_ptr->GetClass();                             \
    }                                                                                \
                                                                                     \
    HYP_FORCE_INLINE static const HypClass* Class()                                  \
    {                                                                                \
        return HypObjectInitializer<T>::GetClass_Static();                           \
    }                                                                                \
                                                                                     \
    template <class TOther>                                                          \
    HYP_FORCE_INLINE bool IsInstanceOf() const                                       \
    {                                                                                \
        if constexpr (std::is_same_v<T, TOther> || std::is_base_of_v<TOther, T>)     \
        {                                                                            \
            return true;                                                             \
        }                                                                            \
        else                                                                         \
        {                                                                            \
            const HypClass* other_hyp_class = GetClass(TypeID::ForType<TOther>());   \
            if (!other_hyp_class)                                                    \
            {                                                                        \
                return false;                                                        \
            }                                                                        \
            return IsInstanceOfHypClass(other_hyp_class, InstanceClass());           \
        }                                                                            \
    }                                                                                \
                                                                                     \
    HYP_FORCE_INLINE bool IsInstanceOf(const HypClass* other_hyp_class) const        \
    {                                                                                \
        if (!other_hyp_class)                                                        \
        {                                                                            \
            return false;                                                            \
        }                                                                            \
        return IsInstanceOfHypClass(other_hyp_class, InstanceClass());               \
    }                                                                                \
                                                                                     \
private:

template <class T, class... Args>
extern inline Handle<T> CreateObject(Args&&...);

template <class T>
extern inline bool InitObject(const Handle<T>&);

class HYP_API HypObjectBase
{
    // template <class T>
    // friend class HypObject;

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

    HYP_FORCE_INLINE HypObjectHeader* GetObjectHeader_Internal() const
    {
        return m_header;
    }

    HYP_FORCE_INLINE bool IsInitCalled() const
    {
        return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_INIT_CALLED;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_init_state.Get(MemoryOrder::RELAXED) & INIT_STATE_READY;
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
        : m_init_state(other.m_init_state.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE)),
          m_delegate_handlers(std::move(other.m_delegate_handlers))
    {
    }

    HypObjectBase& operator=(HypObjectBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_init_state.Set(other.m_init_state.Exchange(INIT_STATE_UNINITIALIZED, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
        m_delegate_handlers = std::move(other.m_delegate_handlers);

        return *this;
    }

    /*! \brief Don't call manually (except for instances of derived types calling base class Init() in their own Init() method). */
    virtual void Init()
    {
        // Do nothing by default.
    }

    IDBase GetID_Internal() const;

    void SetReady(bool is_ready)
    {
        if (is_ready)
        {
            m_init_state.BitOr(INIT_STATE_READY, MemoryOrder::RELAXED);
        }
        else
        {
            m_init_state.BitAnd(~INIT_STATE_READY, MemoryOrder::RELAXED);
        }
    }

    HYP_FORCE_INLINE void AssertReady() const
    {
        AssertThrowMsg(
            IsReady(),
            "Object is not in ready state; maybe Init() has not been called on it, "
            "or the component requires an event to be sent from the Engine instance to determine that "
            "it is ready to be constructed, and this event has not yet been sent.\n");
    }

    HYP_FORCE_INLINE void AssertIsInitCalled() const
    {
        AssertThrowMsg(IsInitCalled(), "Object has not had Init() called on it!");
    }

    void AddDelegateHandler(Name name, DelegateHandler&& delegate_handler)
    {
        m_delegate_handlers.Add(name, std::move(delegate_handler));
    }

    void AddDelegateHandler(DelegateHandler&& delegate_handler)
    {
        m_delegate_handlers.Add(std::move(delegate_handler));
    }

    bool RemoveDelegateHandler(WeakName name)
    {
        return m_delegate_handlers.Remove(name);
    }

    // Pointer to the header of the object, holding container, index and ref counts. Must be the first member.
    HypObjectHeader* m_header;
    DelegateHandlerSet m_delegate_handlers;

private:
    // Used internally by InitObject() to call derived Init() methods.
    void Init_Internal()
    {
        Init();
    }

    AtomicVar<uint16> m_init_state;
};

template <class T>
class HypObject : public HypObjectBase
{
public:
    virtual ~HypObject() = default;

    HYP_FORCE_INLINE ID<T> GetID() const
    {
        return ID<T>(HypObjectBase::GetID_Internal());
    }

    HYP_FORCE_INLINE Handle<T> HandleFromThis() const
    {
        return Handle<T>(const_cast<HypObjectBase*>(static_cast<const HypObjectBase*>(this)));
    }

    HYP_FORCE_INLINE WeakHandle<T> WeakHandleFromThis() const
    {
        return WeakHandle<T>(const_cast<HypObjectBase*>(static_cast<const HypObjectBase*>(this)));
    }
};

} // namespace hyperion

#endif