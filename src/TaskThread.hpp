/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TASK_THREAD_HPP
#define HYPERION_TASK_THREAD_HPP

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/lib/Queue.hpp>
#include <math/MathUtil.hpp>
#include <GameCounter.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion {

struct ThreadID;

class TaskThread : public Thread<Scheduler<Task<void>>>
{
public:
    TaskThread(const ThreadID &thread_id, ThreadPriorityValue priority = ThreadPriorityValue::NORMAL);

    virtual ~TaskThread() override = default;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE bool IsFree() const
        { return m_is_free.Get(MemoryOrder::RELAXED); }

    virtual void Stop();

protected:
    virtual void operator()() override;

    AtomicVar<bool>                 m_is_running;
    AtomicVar<bool>                 m_stop_requested;
    AtomicVar<bool>                 m_is_free;

    Queue<Scheduler::ScheduledTask> m_task_queue;
};

} // namespace hyperion

#endif