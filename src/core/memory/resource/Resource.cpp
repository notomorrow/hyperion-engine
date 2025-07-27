#include <core/memory/resource/Resource.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Memory);
HYP_DEFINE_LOG_SUBCHANNEL(Resource, Memory);

#pragma region Memory Pool

static TypeMap<UniquePtr<IResourceMemoryPool>> g_resourceMemoryPools;
static Mutex g_resourceMemoryPoolsMutex;

HYP_API IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeId typeId, UniquePtr<IResourceMemoryPool> (*createFn)(void))
{
    Mutex::Guard guard(g_resourceMemoryPoolsMutex);

    auto it = g_resourceMemoryPools.Find(typeId);

    if (it == g_resourceMemoryPools.End())
    {
        it = g_resourceMemoryPools.Set(typeId, createFn()).first;
    }

    return it->second.Get();
}

#pragma endregion Memory Pool

#pragma region ResourceBase

ResourceBase::ResourceBase()
    : m_isInitialized(0),
      m_initializationThreadId(ThreadId::Invalid())
{
}

ResourceBase::ResourceBase(ResourceBase&& other) noexcept
    : m_isInitialized(other.m_isInitialized.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_initializationThreadId(other.m_initializationThreadId)
{
    other.m_initializationThreadId = ThreadId::Invalid();
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    HYP_CORE_ASSERT(m_refCounter.IsInSignalState(), "Resource destroyed while still in use, was WaitForFinalization() called?");
}

bool ResourceBase::IsInitialized() const
{
    return m_isInitialized.Get(MemoryOrder::ACQUIRE);
}

int ResourceBase::IncRefNoInitialize()
{
    Mutex::Guard guard(m_refMutex);
    
    int count = m_refCounter.Produce(1);

    if (count == 1)
    {
        m_isInitialized.Set(1, MemoryOrder::RELEASE);
        m_initializationThreadId = Threads::CurrentThreadId();
    }

    return count;
}

int ResourceBase::IncRef()
{
    HYP_SCOPE;

    m_completionSemaphore.Produce(1);
    
    m_refMutex.Lock();

    int result = m_refCounter.Produce(1);

    if (result == 1)
    {
        HYP_NAMED_SCOPE("Initializing Resource - Initialization");

        Assert(!m_isInitialized.Get(MemoryOrder::ACQUIRE));
        
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        m_initializationThreadId = Threads::CurrentThreadId();

        Initialize();
        
        m_isInitialized.Set(1, MemoryOrder::RELEASE);
    }
    
    m_refMutex.Unlock();

    m_completionSemaphore.Release(1);

    return result;
}

int ResourceBase::DecRef()
{
    HYP_SCOPE;

    m_completionSemaphore.Produce(1);
    
    m_refMutex.Lock();

    int result = m_refCounter.Release(1);

    if (result == 0)
    {
        HYP_NAMED_SCOPE("Destroying Resource");

        Assert(m_isInitialized.Get(MemoryOrder::ACQUIRE));
        
        {
            HYP_MT_CHECK_RW(m_dataRaceDetector);

            Destroy();

            m_initializationThreadId = ThreadId::Invalid();
            m_isInitialized.Set(0, MemoryOrder::RELEASE);
        }
    }

    m_refMutex.Unlock();
    
    m_completionSemaphore.Release(1);

    return result;
}

void ResourceBase::WaitForFinalization() const
{
    HYP_SCOPE;

    m_refCounter.Acquire();
}

#pragma endregion ResourceBase

#pragma region NullResource

class NullResource final : public IResource
{
public:
    NullResource() = default;
    NullResource(NullResource&& other) noexcept = default;
    virtual ~NullResource() override = default;

    virtual bool IsNull() const override
    {
        return true;
    }

    virtual int IncRef() override
    {
        return 0;
    }

    virtual int IncRefNoInitialize() override
    {
        return 0;
    }

    virtual int DecRef() override
    {
        return 0;
    }

    virtual void WaitForFinalization() const override
    {
        // Do nothing
    }

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle) override
    {
        HYP_NOT_IMPLEMENTED_VOID();
    }
};

HYP_API IResource& GetNullResource()
{
    static NullResource nullResource;

    return nullResource;
}

#pragma endregion NullResource

} // namespace hyperion
