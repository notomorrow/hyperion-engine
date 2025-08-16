/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/memory/MemoryPool.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace hyperion {

static constexpr uint64 g_initializationMaskInitializedBit = 0x1;
static constexpr uint64 g_initializationMaskReadMask = uint64(-1) & ~g_initializationMaskInitializedBit;

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

template <class T>
struct ResourceMemoryPoolInitInfo : MemoryPoolInitInfo<T>
{
};

struct ResourceMemoryPoolHandle
{
    uint32 index = ~0u;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return index != ~0u;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return index == ~0u;
    }
};

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered, streamed, or otherwise consuming resources.
class IResource
{
public:
    virtual ~IResource() = default;

    virtual bool IsNull() const = 0;

    virtual int IncRef() = 0;
    virtual int IncRefNoInitialize() = 0;
    virtual int DecRef() = 0;

    /*! \brief Waits for ref count to be 0 and all tasks to be completed.
     *  If any ResourceHandle objects are still alive, this will block until they are destroyed.
     *  \note Ensure the current thread does not hold any ResourceHandle objects when calling this function, or it will deadlock. */
    virtual void WaitForFinalization() const = 0;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const = 0;
    virtual void SetPoolHandle(ResourceMemoryPoolHandle poolHandle) = 0;
};

/*! \brief Basic abstract implementation of IResource, using reference counting to manage the lifetime of the underlying resources. */
class HYP_API ResourceBase : public IResource
{
protected:
    using RefCounter = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using InitState = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::ConditionVarSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

    ResourceBase();
    ~ResourceBase();

public:
    ResourceBase(const ResourceBase& other) = delete;
    ResourceBase& operator=(const ResourceBase& other) = delete;
    ResourceBase(ResourceBase&& other) noexcept = delete;
    ResourceBase& operator=(ResourceBase&& other) noexcept = delete;

    HYP_FORCE_INLINE int NumRefs() const
    {
        return m_refCount;
    }

    virtual bool IsNull() const override final
    {
        return false;
    }

    virtual int IncRef() override final;

    // Needed to increment ref count for resources that are initialized in LOADED state.
    // We can't call Initialize() because it is a virtual function and the object might not be fully constructed yet.
    virtual int IncRefNoInitialize() override final;

    virtual int DecRef() override final;

    /*! \brief Wait for the resource to no longer be in loaded state */
    virtual void WaitForFinalization() const override final;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override final
    {
        return m_poolHandle;
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle poolHandle) override final
    {
        m_poolHandle = poolHandle;
    }

    bool IsInitialized() const;

protected:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;

protected:
    int32 m_refCount;
    mutable Mutex m_mutex;

    ResourceMemoryPoolHandle m_poolHandle;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);

private:
    InitState m_initState;
};

class IResourceMemoryPool
{
public:
    virtual ~IResourceMemoryPool() = default;

    virtual void Free(void* ptr) = 0;
};

extern HYP_API IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeId typeId, UniquePtr<IResourceMemoryPool> (*createFn)(void));

template <class T>
class ResourceMemoryPool final : private MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>, public IResourceMemoryPool
{
    static const Name s_poolName;

public:
    static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");

    using Base = MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>;

    static ResourceMemoryPool<T>* GetInstance()
    {
        static IResourceMemoryPool* pool = GetOrCreateResourceMemoryPool(TypeId::ForType<T>(), []() -> UniquePtr<IResourceMemoryPool>
            {
                return MakeUnique<ResourceMemoryPool<T>>();
            });

        return static_cast<ResourceMemoryPool<T>*>(pool);
    }

    ResourceMemoryPool()
        : Base(s_poolName)
    {
    }

    virtual ~ResourceMemoryPool() override = default;

    template <class... Args>
    T* Allocate(Args&&... args)
    {
        ValueStorage<T>* element;
        const uint32 index = Base::AcquireIndex(&element);

        T* ptr = element->Construct(std::forward<Args>(args)...);
        ptr->SetPoolHandle(ResourceMemoryPoolHandle { index });

        return ptr;
    }

    virtual void Free(void* ptr) override
    {
        Free_Internal(reinterpret_cast<T*>(ptr));
    }

private:
    void Free_Internal(T* ptr)
    {
        HYP_CORE_ASSERT(ptr != nullptr);

        ptr->WaitForFinalization();

        const ResourceMemoryPoolHandle poolHandle = ptr->GetPoolHandle();
        HYP_CORE_ASSERT(poolHandle, "Resource has no pool handle set - the resource was likely not allocated using the pool");

        // Invoke the destructor
        ptr->~T();

        Base::ReleaseIndex(poolHandle.index);
    }
};

template <class T>
const Name ResourceMemoryPool<T>::s_poolName = CreateNameFromDynamicString(ANSIString("ResourceMemoryPool_") + TypeNameWithoutNamespace<T>().Data());

template <class T, class... Args>
HYP_FORCE_INLINE static T* AllocateResource(Args&&... args)
{
    return ResourceMemoryPool<T>::GetInstance()->Allocate(std::forward<Args>(args)...);
}

template <class T>
HYP_FORCE_INLINE static void FreeResource(T* ptr)
{
    if (!ptr)
    {
        return;
    }

    ResourceMemoryPool<T>::GetInstance()->Free(ptr);
}

HYP_API IResource& GetNullResource();

class ResourceHandle
{
public:
    ResourceHandle()
        : m_resource(&GetNullResource())
    {
    }

    /*! \brief Construct a ResourceHandle using the given resource. The resource will have its ref count incremented if it is not null.
     *  If \ref{shouldInitialize} is true (default), the resource will be initialized.
     *  Otherwise, IncRefNoInitialize() will be called (this should only be used when required, like in the constructor of base classes that have Initialize() as a virtual method). */
    ResourceHandle(IResource& resource, bool shouldInitialize = true)
        : m_resource(&resource)
    {
        if (!resource.IsNull())
        {
            if (shouldInitialize)
            {
                resource.IncRef();
            }
            else
            {
                resource.IncRefNoInitialize();
            }
        }
    }

    ResourceHandle(const ResourceHandle& other)
        : m_resource(other.m_resource)
    {
        if (!m_resource->IsNull())
        {
            m_resource->IncRef();
        }
    }

    ResourceHandle& operator=(const ResourceHandle& other)
    {
        if (this == &other || m_resource == other.m_resource)
        {
            return *this;
        }

        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }

        m_resource = other.m_resource;

        if (!m_resource->IsNull())
        {
            m_resource->IncRef();
        }

        return *this;
    }

    ResourceHandle(ResourceHandle&& other) noexcept
        : m_resource(other.m_resource)
    {
        other.m_resource = &GetNullResource();
    }

    ResourceHandle& operator=(ResourceHandle&& other) noexcept
    {
        if (this == &other || m_resource == other.m_resource)
        {
            return *this;
        }

        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }

        m_resource = other.m_resource;
        other.m_resource = &GetNullResource();

        return *this;
    }

    ~ResourceHandle()
    {
        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }
    }

    void Reset()
    {
        if (!m_resource->IsNull())
        {
            m_resource->DecRef();

            m_resource = &GetNullResource();
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return !m_resource->IsNull();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_resource->IsNull();
    }

    HYP_FORCE_INLINE bool operator==(const ResourceHandle& other) const
    {
        return m_resource == other.m_resource;
    }

    HYP_FORCE_INLINE bool operator!=(const ResourceHandle& other) const
    {
        return m_resource != other.m_resource;
    }

    HYP_FORCE_INLINE IResource* operator->() const
    {
        return m_resource;
    }

    HYP_FORCE_INLINE IResource& operator*() const
    {
        HYP_CORE_ASSERT(!m_resource->IsNull());

        return *m_resource;
    }

protected:
    IResource* m_resource;
};

template <class ResourceType>
class TResourceHandle : public ResourceHandle
{
public:
    TResourceHandle() = default;

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<ResourceHandle, NormalizedType<T>>>>
    TResourceHandle(T& resource)
        : ResourceHandle(static_cast<IResource&>(resource))
    {
    }

    TResourceHandle(const TResourceHandle& other)
        : ResourceHandle(static_cast<const ResourceHandle&>(other))
    {
    }

    TResourceHandle& operator=(const TResourceHandle& other)
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceHandle::operator=(static_cast<const ResourceHandle&>(other));

        return *this;
    }

    TResourceHandle(TResourceHandle&& other) noexcept
        : ResourceHandle(static_cast<ResourceHandle&&>(std::move(other)))
    {
    }

    TResourceHandle& operator=(TResourceHandle&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceHandle::operator=(static_cast<ResourceHandle&&>(std::move(other)));

        return *this;
    }

    ~TResourceHandle() = default;

    HYP_FORCE_INLINE ResourceType* Get() const
    {
        static_assert(std::is_base_of_v<IResource, ResourceType>);

        if (m_resource->IsNull())
        {
            return nullptr;
        }

        // can safely cast to ResourceType since we know it's not NullResource
        return static_cast<ResourceType*>(m_resource);
    }

    HYP_FORCE_INLINE ResourceType* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE ResourceType& operator*() const
    {
        ResourceType* ptr = Get();

        if (!ptr)
        {
            HYP_FAIL("Dereferencing null resource handle!");
        }

        return *ptr;
    }
};

} // namespace hyperion
