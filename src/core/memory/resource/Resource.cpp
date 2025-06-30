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
    : m_initializationMask(0),
      m_initializationThreadId(ThreadId::Invalid()),
      m_updateCounter(0)
{
}

ResourceBase::ResourceBase(ResourceBase&& other) noexcept
    : m_initializationMask(other.m_initializationMask.Exchange(0, MemoryOrder::ACQUIRE_RELEASE)),
      m_initializationThreadId(other.m_initializationThreadId),
      m_updateCounter(other.m_updateCounter.Exchange(0, MemoryOrder::ACQUIRE_RELEASE))
{
    other.m_initializationThreadId = ThreadId::Invalid();
}

ResourceBase::~ResourceBase()
{
    // Ensure that the resources are no longer being used
    AssertThrowMsg(m_refCounter.IsInSignalState(), "Resource destroyed while still in use, was WaitForFinalization() called?");
}

bool ResourceBase::IsInitialized() const
{
    return m_initializationMask.Get(MemoryOrder::ACQUIRE) & g_initializationMaskInitializedBit;
}

int ResourceBase::IncRefNoInitialize()
{
    int count = m_refCounter.Produce(1);

    if (count == 1)
    {
        // loop until we have exclusive access.
        uint64 state = m_initializationMask.BitOr(g_initializationMaskInitializedBit, MemoryOrder::ACQUIRE);
        AssertDebugMsg(!(state & g_initializationMaskInitializedBit), "Attempted to initialize a resource that is already initialized");

        while (state & g_initializationMaskReadMask)
        {
            state = m_initializationMask.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }

        m_initializationThreadId = Threads::CurrentThreadId();
    }

    return count;
}

int ResourceBase::IncRef()
{
    ThreadBase* ownerThread = GetOwnerThread();

    auto impl = [this]()
    {
        {
            HYP_NAMED_SCOPE("Initializing Resource - Initialization");

            uint64 state = m_initializationMask.BitOr(g_initializationMaskInitializedBit, MemoryOrder::ACQUIRE_RELEASE);
            AssertDebugMsg(!(state & g_initializationMaskInitializedBit), "Attempted to initialize a resource that is already initialized! Initialization thread id: %s (%u)",
                m_initializationThreadId.GetName().LookupString(), m_initializationThreadId.GetValue());

            // loop until we have exclusive access.
            while (state & g_initializationMaskReadMask)
            {
                state = m_initializationMask.Get(MemoryOrder::ACQUIRE);
                Threads::Sleep(0);
            }

            HYP_MT_CHECK_RW(m_dataRaceDetector);

            m_initializationThreadId = Threads::CurrentThreadId();

            Initialize();
        }

        {
            HYP_NAMED_SCOPE("Initializing Resource - Post-Initialization Update");

            // Perform any updates that were requested before initialization
            int16 currentUpdateCount = m_updateCounter.Get(MemoryOrder::ACQUIRE);

            while (currentUpdateCount != 0)
            {
                AssertThrow(currentUpdateCount > 0);

                Update();

                currentUpdateCount = m_updateCounter.Decrement(currentUpdateCount, MemoryOrder::ACQUIRE_RELEASE) - currentUpdateCount;
            }
        }

        m_completionSemaphore.Release(1);
    };

    HYP_SCOPE;

    m_completionSemaphore.Produce(1);
    bool shouldRelease = true;

    int result = m_refCounter.Produce(1);

    if (result == 1)
    {
        shouldRelease = false;

        if (CanExecuteInline())
        {
            impl();
        }
        else
        {
            EnqueueOp(std::move(impl));
        }
    }

    if (shouldRelease)
    {
        m_completionSemaphore.Release(1);
    }

    return result;
}

int ResourceBase::DecRef()
{
    auto impl = [this]()
    {
        HYP_NAMED_SCOPE("Destroying Resource");

        uint64 state = m_initializationMask.BitOr(g_initializationMaskInitializedBit, MemoryOrder::ACQUIRE_RELEASE);
        AssertDebugMsg(state & g_initializationMaskInitializedBit, "ResourceBase::DecRef() called on a resource that is not initialized!");

        // Wait till all reads are complete before we continue
        while (state & g_initializationMaskReadMask)
        {
            state = m_initializationMask.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }

        // While we still have the initialized bit set, add the read access, so Initialize doesn't happen while we are destroying
        m_initializationMask.Increment(2, MemoryOrder::RELEASE);
        // Unset the initialized bit
        m_initializationMask.BitAnd(~g_initializationMaskInitializedBit, MemoryOrder::RELEASE);

        // DEBUGGING
        state = m_initializationMask.Get(MemoryOrder::ACQUIRE);
        AssertDebugMsg(!(state & g_initializationMaskInitializedBit), "ResourceBase::DecRef() called on a resource that is still initialized! Initialization thread id: %s (%u)",
            m_initializationThreadId.GetName().LookupString(), m_initializationThreadId.GetValue());

        {
            HYP_MT_CHECK_RW(m_dataRaceDetector);

            Destroy();

            m_initializationThreadId = ThreadId::Invalid();
        }

        // Done with destroying, we can now remove our read access
        m_initializationMask.Decrement(2, MemoryOrder::RELEASE);

        m_completionSemaphore.Release(1);
    };

    HYP_SCOPE;

    m_completionSemaphore.Produce(1);
    bool shouldRelease = true;

    int result = m_refCounter.Release(1);

    if (result == 0)
    {
        shouldRelease = false;

        if (CanExecuteInline())
        {

            impl();
        }
        else
        {
            EnqueueOp(std::move(impl));
        }
    }

    if (shouldRelease)
    {
        m_completionSemaphore.Release(1);
    }

    return result;
}

void ResourceBase::SetNeedsUpdate()
{
    auto impl = [this]()
    {
        HYP_NAMED_SCOPE("Applying Resource updates");

        {
            HYP_MT_CHECK_READ(m_dataRaceDetector);

            int16 currentCount = m_updateCounter.Get(MemoryOrder::ACQUIRE);

            while (currentCount != 0)
            {
                AssertDebug(currentCount > 0);

                HYP_MT_CHECK_RW(m_dataRaceDetector);

                Update();

                currentCount = m_updateCounter.Decrement(currentCount, MemoryOrder::ACQUIRE_RELEASE) - currentCount;
            }
        }

        m_completionSemaphore.Release(1);
    };

    HYP_SCOPE;

    m_completionSemaphore.Produce(1);

    if (m_refCounter.IsInSignalState())
    {
        // Check initialization state -- if it is not initialized, we increment the update counter
        // without waiting for the owner thread to finish
        if (!(m_initializationMask.Increment(2, MemoryOrder::ACQUIRE) & g_initializationMaskInitializedBit))
        {
            m_updateCounter.Increment(1, MemoryOrder::RELEASE);

            // Remove our read access
            m_initializationMask.Decrement(2, MemoryOrder::RELAXED);

            m_completionSemaphore.Release(1);

            // No work to be done yet since we aren't initialized.
            return;
        }

        // Remove our read access
        m_initializationMask.Decrement(2, MemoryOrder::RELAXED);
    }

    m_updateCounter.Increment(1, MemoryOrder::RELEASE);

    if (!CanExecuteInline())
    {
        EnqueueOp(std::move(impl));

        return;
    }

    impl();
}

void ResourceBase::WaitForTaskCompletion() const
{
    HYP_SCOPE;

    if (CanExecuteInline())
    {
        if (!m_completionSemaphore.IsInSignalState())
        {
            if (GetOwnerThread() == nullptr)
            { // No owner thread is used to execute tasks on.
                HYP_FAIL("No owner thread - cannot wait for task completion");
            }
            else
            { // We are on the owner thread
                // We need to flush pending tasks if we are on the owner thread and
                // we still have pending tasks.
                HYP_NAMED_SCOPE("Flushing scheduler tasks");

                HYP_LOG(Resource, Debug, "Flushing scheduler tasks for Resource");

                FlushScheduledTasks();
            }

            AssertThrow(m_completionSemaphore.IsInSignalState());
        }

        return;
    }

    // Wait for tasks to complete on another thread
    m_completionSemaphore.Acquire();
}

void ResourceBase::WaitForFinalization() const
{
    HYP_SCOPE;

    WaitForTaskCompletion();

    m_refCounter.Acquire();
}

bool ResourceBase::CanExecuteInline() const
{
    ThreadBase* ownerThread = GetOwnerThread();

    return ownerThread == nullptr || Threads::IsOnThread(ownerThread->Id());
}

void ResourceBase::FlushScheduledTasks() const
{
    ThreadBase* ownerThread = GetOwnerThread();
    AssertThrow(ownerThread != nullptr);

    ownerThread->GetScheduler().Flush([](auto& operation)
        {
            operation.Execute();
        });
}

void ResourceBase::EnqueueOp(Proc<void()>&& proc)
{
    GetOwnerThread()->GetScheduler().Enqueue(std::move(proc), TaskEnqueueFlags::FIRE_AND_FORGET);
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

    virtual void WaitForTaskCompletion() const override
    {
        // Do nothing
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