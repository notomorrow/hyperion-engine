/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TASK_THREAD_HPP
#define HYPERION_TASK_THREAD_HPP

#include <core/threading/Thread.hpp>
#include <core/threading/Scheduler.hpp>
#include <core/containers/Queue.hpp>
#include <math/MathUtil.hpp>
#include <GameCounter.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace threading {

struct ThreadID;

class TaskThread : public Thread<Scheduler<Task<void>>>
{
public:
    TaskThread(const ThreadID &thread_id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    virtual ~TaskThread() override = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsFree() const
        { return m_num_tasks.Get(MemoryOrder::RELAXED) == 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 NumTasks() const
        { return m_num_tasks.Get(MemoryOrder::RELAXED); }

    virtual void Stop();

protected:
    /*! \brief Method to be executed each tick of the task thread, before executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void BeforeExecuteTasks() { }

    /*! \brief Method to be executed each tick of the task thread, after executing tasks.
     *  Used by derived classes to inject custom logic. */
    virtual void AfterExecuteTasks() { }

    virtual void operator()() override;

    AtomicVar<bool>                 m_is_running;
    AtomicVar<bool>                 m_stop_requested;
    AtomicVar<uint32>               m_num_tasks;

    Queue<Scheduler::ScheduledTask> m_task_queue;
};

} // namespace threading

using threading::TaskThread;

} // namespace hyperion

#endif