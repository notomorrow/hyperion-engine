/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_OBJECT_POOL_HPP
#define HYPERION_CORE_OBJECT_POOL_HPP

#include <core/IDGenerator.hpp>
#include <core/ID.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Memory.hpp>
#include <core/memory/MemoryPool.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace hyperion {

class Engine;

template <class T>
struct HandleDefinition;

template <class T>
class ObjectContainer;

class IObjectContainer;

struct HypObjectHeader;
class HypClass;

class IObjectContainer
{
public:
    virtual ~IObjectContainer() = default;

    virtual SizeType NumAllocatedElements() const = 0;
    virtual SizeType NumAllocatedBytes() const = 0;

    virtual void IncRefStrong(HypObjectHeader*) = 0;
    virtual void IncRefWeak(HypObjectHeader*) = 0;
    virtual void DecRefStrong(HypObjectHeader*) = 0;
    virtual void DecRefWeak(HypObjectHeader*) = 0;
    virtual void* Release(HypObjectHeader*) = 0;

    virtual void* GetObjectPointer(HypObjectHeader*) = 0;
    virtual HypObjectHeader* GetObjectHeader(uint32 index) = 0;
    virtual TypeID GetObjectTypeID() const = 0;
    virtual const HypClass* GetObjectClass() const = 0;

    virtual void ReleaseIndex(uint32 index) = 0;
};

/*! \brief Metadata for a generic object in the object pool. */
struct HypObjectHeader
{
    IObjectContainer* container;
    uint32 index;
    AtomicVar<uint32> ref_count_strong;
    AtomicVar<uint32> ref_count_weak;

#ifdef HYP_DEBUG_MODE
    AtomicVar<bool> has_value;
#endif

    HypObjectHeader()
        : container(nullptr),
          index(~0u),
          ref_count_strong(0),
          ref_count_weak(0)
    {
#ifdef HYP_DEBUG_MODE
        has_value.Set(false, MemoryOrder::SEQUENTIAL);
#endif
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
        return ref_count_strong.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
    {
        return ref_count_weak.Get(MemoryOrder::ACQUIRE);
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

    alignas(T) ubyte bytes[sizeof(T)];

    HypObjectMemory() = default;
    HypObjectMemory(const HypObjectMemory&) = delete;
    HypObjectMemory& operator=(const HypObjectMemory&) = delete;
    HypObjectMemory(HypObjectMemory&&) noexcept = delete;
    HypObjectMemory& operator=(HypObjectMemory&&) noexcept = delete;
    ~HypObjectMemory() = default;

    template <class... Args>
    T* Construct(Args&&... args)
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(!has_value.Exchange(true, MemoryOrder::SEQUENTIAL));
#endif

        T* ptr = GetPointer();

        // Note: don't use Memory::ConstructWithContext as we need to set the header pointer before HypObjectInitializerGuard destructs
        HypObjectInitializerGuard<T> context { ptr };

        // Set the object header to point to this
        ptr->HypObjectBase::m_header = static_cast<HypObjectHeader*>(this);

        Memory::Construct<T>(static_cast<void*>(ptr), std::forward<Args>(args)...);

        return ptr;
    }

    uint32 IncRefStrong()
    {
        const uint32 count = ref_count_strong.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;

        HypObject_OnIncRefCount_Strong(HypObjectPtr(GetPointer()), count);

        return count;
    }

    uint32 IncRefWeak()
    {
        return ref_count_weak.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
    }

    uint32 DecRefStrong()
    {
        uint32 count;

        if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
#ifdef HYP_DEBUG_MODE
            AssertThrow(has_value.Exchange(false, MemoryOrder::SEQUENTIAL));

            AssertThrow(container != nullptr);
            AssertThrow(index != ~0u);
#endif

            // Increment weak reference count by 1 so any WeakHandleFromThis() calls in the destructor do not immediately cause FreeID() to be called.
            ref_count_weak.Increment(1, MemoryOrder::RELEASE);

            HypObject_OnDecRefCount_Strong(HypObjectPtr(GetPointer()), count - 1);

            GetPointer()->~T();

            if (ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) == 1)
            {
#ifdef HYP_DEBUG_MODE
                AssertThrow(!has_value.Get(MemoryOrder::SEQUENTIAL));
#endif

                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }
        else
        {
            HypObject_OnDecRefCount_Strong(HypObjectPtr(GetPointer()), count - 1);
        }

        return count - 1;
    }

    uint32 DecRefWeak()
    {
        uint32 count;

        if ((count = ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
#ifdef HYP_DEBUG_MODE
            AssertThrow(container != nullptr);
            AssertThrow(index != ~0u);
#endif

            if (ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0)
            {
#ifdef HYP_DEBUG_MODE
                AssertThrow(!has_value.Get(MemoryOrder::SEQUENTIAL));
#endif

                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }

        AssertDebug(count != 0);

        return count - 1;
    }

    HYP_NODISCARD T* Release()
    {
        T* ptr = reinterpret_cast<T*>(&bytes[0]);

#ifdef HYP_DEBUG_MODE
        AssertThrow(has_value.Get(MemoryOrder::SEQUENTIAL));

        AssertThrow(container != nullptr);
        AssertThrow(index != ~0u);
#endif

        return ptr;
    }

    HYP_FORCE_INLINE T& Get()
    {
        return *reinterpret_cast<T*>(&bytes[0]);
    }

    HYP_FORCE_INLINE T* GetPointer()
    {
        return reinterpret_cast<T*>(&bytes[0]);
    }

    HYP_FORCE_INLINE const T* GetPointer() const
    {
        return reinterpret_cast<const T*>(&bytes[0]);
    }

    /*! \brief Gets the offset past the header (in bytes) of the memory of the actual object held by a HypObjectMemory<T>. */
    HYP_FORCE_INLINE static constexpr uintptr_t GetAlignedOffset()
    {
        return ((sizeof(HypObjectHeader) + alignof(T) - 1) / alignof(T)) * alignof(T);
    }
};

template <class T>
static inline void ObjectContainer_OnBlockAllocated(void* ctx, HypObjectMemory<T>* elements, uint32 offset, uint32 count)
{
    IObjectContainer* container = static_cast<ObjectContainer<T>*>(ctx);
    AssertThrow(container != nullptr);

    for (uint32 index = 0; index < count; index++)
    {
        elements[index].container = container;
        elements[index].index = offset + index;
    }
}

template <class T>
class ObjectContainer final : public IObjectContainer
{
    using MemoryPoolType = MemoryPool<HypObjectMemory<T>, MemoryPoolInitInfo, ObjectContainer_OnBlockAllocated<T>>;

    using HypObjectMemory = HypObjectMemory<T>;

public:
    ObjectContainer()
        : m_pool(2048, /* create_initial_blocks */ true, /* block_init_ctx */ this)
    {
        // Initialize the default / null object
        m_default_object.container = this;
        m_default_object.index = ~0u;
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
        uint32 index = m_pool.AcquireIndex(&element);

#ifdef HYP_DEBUG_MODE // sanity checks
        AssertThrow(uintptr_t(element->container) == uintptr_t(this));
        AssertThrow(element->index == index);
        AssertThrow(element->ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0);
        AssertThrow(element->ref_count_weak.Get(MemoryOrder::ACQUIRE) == 0);
        AssertThrow(!element->has_value.Get(MemoryOrder::ACQUIRE));
#endif

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

    virtual void* GetObjectPointer(HypObjectHeader* ptr) override
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

    virtual TypeID GetObjectTypeID() const override
    {
        static const TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    virtual const HypClass* GetObjectClass() const override
    {
        static const HypClass* hyp_class = GetClass(TypeID::ForType<T>());

        return hyp_class;
    }

    virtual void ReleaseIndex(uint32 index) override
    {
        m_pool.ReleaseIndex(index);
    }

    // private:
    MemoryPoolType m_pool;
    HypObjectMemory m_default_object;
};

class ObjectPool
{
public:
    class ObjectContainerMap
    {
        // Maps type ID to object container
        // Use a linked list so that references are never invalidated.
        LinkedList<Pair<TypeID, UniquePtr<IObjectContainer>>> m_map;
        Mutex m_mutex;

    public:
        ObjectContainerMap() = default;
        ObjectContainerMap(const ObjectContainerMap&) = delete;
        ObjectContainerMap& operator=(const ObjectContainerMap&) = delete;
        ObjectContainerMap(ObjectContainerMap&&) noexcept = delete;
        ObjectContainerMap& operator=(ObjectContainerMap&&) noexcept = delete;
        ~ObjectContainerMap() = default;

        template <class T>
        ObjectContainer<T>& GetOrCreate()
        {
            // static variable to ensure that the object container is only created once and we don't have to lock everytime this is called
            static ObjectContainer<T>& container = static_cast<ObjectContainer<T>&>(GetOrCreate(TypeID::ForType<T>(), []() -> UniquePtr<IObjectContainer>
                {
                    return MakeUnique<ObjectContainer<T>>();
                }));

            return container;
        }

        HYP_API IObjectContainer& Add(TypeID type_id);

        HYP_API IObjectContainer& Get(TypeID type_id);
        HYP_API IObjectContainer* TryGet(TypeID type_id);

    private:
        IObjectContainer& GetOrCreate(TypeID type_id, UniquePtr<IObjectContainer> (*create_fn)(void))
        {
            Mutex::Guard guard(m_mutex);

            auto it = m_map.FindIf([type_id](const auto& element)
                {
                    return element.first == type_id;
                });

            if (it != m_map.End())
            {
                if (it->second == nullptr)
                {
                    it->second = create_fn();
                }

                return *it->second;
            }

            UniquePtr<IObjectContainer> container = create_fn();
            AssertThrow(container != nullptr);

            return *m_map.EmplaceBack(type_id, std::move(container)).second;
        }
    };

    HYP_API static ObjectContainerMap& GetObjectContainerHolder();
};

} // namespace hyperion

#endif