/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TASK_THREAD_HPP
#define HYPERION_TASK_THREAD_HPP

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/containers/Queue.hpp>
#include <core/math/MathUtil.hpp>
#include <GameCounter.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace threading {

class ThreadId;

class HYP_API TaskThread : public Thread<Scheduler>
{
public:
    TaskThread(const ThreadId& threadId, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);
    TaskThread(Name name, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    virtual ~TaskThread() override = default;

    void SetPriority(ThreadPriorityValue priority);

    HYP_FORCE_INLINE bool IsFree() const
    {
        return NumTasks() == 0;
    }

    HYP_FORCE_INLINE uint32 NumTasks() const
    {
        return m_numTasks.Get(MemoryOrder::ACQUIRE);
    }

protected:
    /*! \brief Method to be executed each tick of the task thread, before executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void BeforeExecuteTasks()
    {
    }

    /*! \brief Method to be executed each tick of the task thread, after executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void AfterExecuteTasks()
    {
    }

    virtual void operator()() override;

    AtomicVar<uint32> m_numTasks;

    Queue<Scheduler::ScheduledTask> m_taskQueue;
};

} // namespace threading

using threading::TaskThread;

} // namespace hyperion

#endif