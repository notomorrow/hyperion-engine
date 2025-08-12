/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Memory.hpp>
#include <core/memory/MemoryPool.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace hyperion {

template <class T>
class ObjectContainer;

class ObjectContainerBase;

struct HypObjectHeader;
class HypClass;

class ObjectContainerBase
{
public:
    virtual ~ObjectContainerBase() = default;

    HYP_FORCE_INLINE const TypeId& GetObjectTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return m_hypClass;
    }

    virtual SizeType NumAllocatedElements() const = 0;
    virtual SizeType NumAllocatedBytes() const = 0;

    virtual void IncRefStrong(HypObjectHeader*) = 0;
    virtual void IncRefWeak(HypObjectHeader*) = 0;
    virtual void DecRefStrong(HypObjectHeader*) = 0;
    virtual void DecRefWeak(HypObjectHeader*) = 0;
    virtual void* Release(HypObjectHeader*) = 0;

    virtual HypObjectBase* GetObjectPointer(HypObjectHeader*) = 0;
    virtual HypObjectHeader* GetObjectHeader(uint32 index) = 0;

    virtual void ReleaseIndex(uint32 index) = 0;

protected:
    ObjectContainerBase(TypeId typeId, const HypClass* hypClass)
        : m_typeId(typeId),
          m_hypClass(hypClass)
    {
        HYP_CORE_ASSERT(typeId != TypeId::Void());
        // HYP_CORE_ASSERT(hypClass != nullptr);
    }

    TypeId m_typeId;
    const HypClass* m_hypClass;
};

/*! \brief Metadata for a generic object in the object pool. */
struct HypObjectHeader
{
    ObjectContainerBase* container;
    uint32 index;
    AtomicVar<uint32> refCountStrong;
    AtomicVar<uint32> refCountWeak;

    HypObjectHeader()
        : container(nullptr),
          index(~0u),
          refCountStrong(0),
          refCountWeak(0)
    {
    }

    HypObjectHeader(const HypObjectHeader&) = delete;
    HypObjectHeader& operator=(const HypObjectHeader&) = delete;
    HypObjectHeader(HypObjectHeader&&) noexcept = delete;
    HypObjectHeader& operator=(HypObjectHeader&&) noexcept = delete;
    ~HypObjectHeader() = default;

    HYP_FORCE_INLINE bool IsNull() const
    {
        return index == ~0u;
    }

    HYP_FORCE_INLINE uint32 GetRefCountStrong() const
    {
        return refCountStrong.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
    {
        return refCountWeak.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE void IncRefStrong()
    {
        container->IncRefStrong(this);
    }

    HYP_FORCE_INLINE void IncRefWeak()
    {
        container->IncRefWeak(this);
    }

    HYP_FORCE_INLINE void DecRefStrong()
    {
        container->DecRefStrong(this);
    }

    HYP_FORCE_INLINE void DecRefWeak()
    {
        container->DecRefWeak(this);
    }

    HYP_FORCE_INLINE void* Release()
    {
        return container->Release(this);
    }
};

/*! \brief Memory storage for T where T is a subclass of HypObjectBase.
 *  Derives from HypObjectHeader to store reference counts and other information at the start of the memory. */
template <class T>
struct HypObjectMemory final : HypObjectHeader
{
    static_assert(std::is_base_of_v<HypObjectBase, T>, "T must be a subclass of HypObjectBase");

    ValueStorage<T> storage;

    HypObjectMemory() = default;
    HypObjectMemory(const HypObjectMemory&) = delete;
    HypObjectMemory& operator=(const HypObjectMemory&) = delete;
    HypObjectMemory(HypObjectMemory&&) noexcept = delete;
    HypObjectMemory& operator=(HypObjectMemory&&) noexcept = delete;
    ~HypObjectMemory() = default;

    uint32 IncRefStrong()
    {
        const uint32 count = refCountStrong.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;

        if (count > 1)
        {
            HypObject_AcquireManagedObjectLock(HypObjectPtr(GetPointer()));
        }

        return count;
    }

    uint32 IncRefWeak()
    {
        return refCountWeak.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
    }

    uint32 DecRefStrong()
    {
        uint32 count;

        if ((count = refCountStrong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            // Increment weak reference count by 1 so any WeakHandleFromThis() calls in the destructor do not immediately cause the item to be removed from the pool
            refCountWeak.Increment(1, MemoryOrder::RELEASE);

            GetPointer()->~T();

            if (refCountWeak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) == 1)
            {
                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }
        else
        {
            HypObject_ReleaseManagedObjectLock(HypObjectPtr(GetPointer()));
        }

        return count - 1;
    }

    uint32 DecRefWeak()
    {
        uint32 count;

        if ((count = refCountWeak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            if (refCountStrong.Get(MemoryOrder::ACQUIRE) == 0)
            {
                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }

        HYP_CORE_ASSERT(count != 0);

        return count - 1;
    }

    HYP_NODISCARD T* Release()
    {
        T* ptr = storage.GetPointer();

        return ptr;
    }

    HYP_FORCE_INLINE T& Get()
    {
        return storage.Get();
    }

    HYP_FORCE_INLINE T* GetPointer()
    {
        return storage.GetPointer();
    }

    HYP_FORCE_INLINE const T* GetPointer() const
    {
        return storage.GetPointer();
    }
};

template <class T>
static inline void ObjectContainer_OnBlockAllocated(void* ctx, HypObjectMemory<T>* elements, uint32 offset, uint32 count)
{
    ObjectContainerBase* container = static_cast<ObjectContainer<T>*>(ctx);
    HYP_CORE_ASSERT(container != nullptr);

    for (uint32 index = 0; index < count; index++)
    {
        elements[index].container = container;
        elements[index].index = offset + index;
    }
}

template <class T>
class ObjectContainer final : public ObjectContainerBase
{
    using MemoryPoolType = MemoryPool<HypObjectMemory<T>, MemoryPoolInitInfo<T>, ObjectContainer_OnBlockAllocated<T>>;

    using HypObjectMemory = HypObjectMemory<T>;

public:
    ObjectContainer()
        : ObjectContainerBase(TypeId::ForType<T>(), T::Class()),
          m_pool(2048, /* createInitialBlocks */ true, /* blockInitCtx */ this)
    {
    }

    ObjectContainer(const ObjectContainer& other) = delete;
    ObjectContainer& operator=(const ObjectContainer& other) = delete;
    ObjectContainer(ObjectContainer&& other) noexcept = delete;
    ObjectContainer& operator=(ObjectContainer&& other) noexcept = delete;
    virtual ~ObjectContainer() override = default;

    virtual SizeType NumAllocatedElements() const override
    {
        return m_pool.NumAllocatedElements();
    }

    virtual SizeType NumAllocatedBytes() const override
    {
        return m_pool.NumAllocatedBytes();
    }

    HYP_NODISCARD HYP_FORCE_INLINE HypObjectMemory* Allocate()
    {
        HypObjectMemory* element;
        m_pool.AcquireIndex(&element);
        AssertDebug(element->GetRefCountStrong() == 0 && element->GetRefCountWeak() == 0,
            "HypObjectMemory should not have any references when allocated from the pool!!");

        return element;
    }

    virtual void IncRefStrong(HypObjectHeader* ptr) override
    {
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::IncRefStrong();
    }

    virtual void IncRefWeak(HypObjectHeader* ptr) override
    {
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::IncRefWeak();
    }

    virtual void DecRefStrong(HypObjectHeader* ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::DecRefStrong();
    }

    virtual void DecRefWeak(HypObjectHeader* ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::DecRefWeak();
    }

    virtual void* Release(HypObjectHeader* ptr) override
    {
        return static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::Release();
    }

    virtual HypObjectBase* GetObjectPointer(HypObjectHeader* ptr) override
    {
        if (!ptr)
        {
            return nullptr;
        }

        return static_cast<HypObjectMemory*>(ptr)->GetPointer();
    }

    virtual HypObjectHeader* GetObjectHeader(uint32 index) override
    {
        return &m_pool.GetElement(index);
    }

    virtual void ReleaseIndex(uint32 index) override
    {
        m_pool.ReleaseIndex(index);
    }

    // private:
    MemoryPoolType m_pool;
};

class ObjectPool
{
public:
    class ObjectContainerMap
    {
        // Maps type Id to object container
        // Use a linked list so that references are never invalidated.
        LinkedList<Pair<TypeId, ObjectContainerBase*>> m_map;
        Mutex m_mutex;

    public:
        ObjectContainerMap() = default;
        ObjectContainerMap(const ObjectContainerMap&) = delete;
        ObjectContainerMap& operator=(const ObjectContainerMap&) = delete;
        ObjectContainerMap(ObjectContainerMap&&) noexcept = delete;
        ObjectContainerMap& operator=(ObjectContainerMap&&) noexcept = delete;
        HYP_API ~ObjectContainerMap();

        template <class T>
        ObjectContainer<T>& GetOrCreate()
        {
            // static variable to ensure that the object container is only created once and we don't have to lock everytime this is called
            static ObjectContainer<T>& container = static_cast<ObjectContainer<T>&>(GetOrCreate(TypeId::ForType<T>(), []() -> ObjectContainerBase*
                {
                    return new ObjectContainer<T>();
                }));

            return container;
        }

        HYP_API ObjectContainerBase& Get(TypeId typeId);
        HYP_API ObjectContainerBase* TryGet(TypeId typeId);

    private:
        HYP_API ObjectContainerBase& GetOrCreate(TypeId typeId, ObjectContainerBase* (*createFn)(void));
    };

    HYP_API static ObjectContainerMap& GetObjectContainerMap();
};

} // namespace hyperion
