/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/threading/Task.hpp>
#include <core/threading/Scheduler.hpp>

namespace hyperion {
namespace threading {

#pragma region TaskCallbackChain

TaskCallbackChain::TaskCallbackChain(TaskCallbackChain&& other) noexcept
{
    Mutex::Guard guard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_numCallbacks.Set(other.m_numCallbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);
}

TaskCallbackChain& TaskCallbackChain::operator=(TaskCallbackChain&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Mutex::Guard thisGuard(m_mutex);
    Mutex::Guard otherGuard(other.m_mutex);

    m_callbacks = std::move(other.m_callbacks);
    m_numCallbacks.Set(other.m_numCallbacks.Exchange(0, MemoryOrder::ACQUIRE_RELEASE), MemoryOrder::RELEASE);

    return *this;
}

void TaskCallbackChain::Add(Proc<void()>&& callback)
{
    // @TODO: Smarter implementation possibly using semaphores that are set up with a value when task is first initialized,
    // need a way to tell if the added callback will never be executed because the task completed.
    Mutex::Guard guard(m_mutex);

    m_callbacks.PushBack(std::move(callback));

    m_numCallbacks.Increment(1, MemoryOrder::RELEASE);
}

void TaskCallbackChain::operator()()
{
    if (m_numCallbacks.Get(MemoryOrder::ACQUIRE))
    {
        Mutex::Guard guard(m_mutex);

        for (Proc<void()>& proc : m_callbacks)
        {
            proc();
        }
    }
}

#pragma endregion TaskCallbackChain

#pragma region TaskBase

bool TaskBase::Cancel()
{
    if (!m_id.IsValid() || !m_assignedScheduler)
    {
        return false;
    }

    if (m_assignedScheduler->Dequeue(m_id))
    {
        m_id = {};
        m_assignedScheduler = nullptr;

        // Reset the task state since it was dequeued.
        Reset();

        return true;
    }

    return false;
}

void TaskBase::Await_Internal() const
{
    HYP_CORE_ASSERT(IsValid());

    TaskExecutorBase* executor = GetTaskExecutor();
    HYP_CORE_ASSERT(executor != nullptr);

    executor->GetNotifier().Await();

#ifdef HYP_DEBUG_MODE
    HYP_CORE_ASSERT(IsCompleted());
#endif
}

#pragma endregion TaskBase

} // namespace threading
} // namespace hyperion