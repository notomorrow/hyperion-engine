#ifndef HYPERION_V2_TASK_THREAD_H
#define HYPERION_V2_TASK_THREAD_H

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/lib/Queue.hpp>
#include <math/MathUtil.hpp>
#include <GameCounter.hpp>
#include <util/Defines.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

struct ThreadID;

class TaskThread : public Thread<Scheduler<Task<void>>>
{
public:
    TaskThread(const ThreadID &thread_id, UInt target_ticks_per_second = 0)
        : Thread(thread_id),
          m_is_running(false),
          m_target_ticks_per_second(target_ticks_per_second)
    {
    }

    virtual ~TaskThread() = default;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_is_running.load(std::memory_order_relaxed); }

    HYP_FORCE_INLINE void Stop()
        { m_is_running.store(false, std::memory_order_relaxed); }

protected:
    virtual void operator()() override
    {
        m_is_running.store(true);

        while (!IsRunning()) {
            HYP_WAIT_IDLE();
        }

        const bool is_locked = m_target_ticks_per_second != 0;
        LockstepGameCounter counter(is_locked ? (1.0f / static_cast<Float>(m_target_ticks_per_second)) : 1.0f);

        UInt idling_counter = 0;
        bool is_in_background_mode = false;

#ifdef HYP_WINDOWS
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif

        while (IsRunning()) {
            if (is_locked) {
                while (counter.Waiting()) {
                    HYP_WAIT_IDLE();
                }

                counter.NextTick();
            }

            bool active = false;

            if (m_scheduler.NumEnqueued()) {
                m_scheduler.AcceptAll(m_task_queue);

                active = true;
            }
            
            // do not execute within lock
            if (m_task_queue.Any()) {
                m_task_queue.Front().Execute();
                m_task_queue.Pop();

                active = true;
            }

            if (active) {
                if (is_in_background_mode) {
#ifdef HYP_WINDOWS
                    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_END);
#endif
                    is_in_background_mode = false;
                    DebugLog(LogType::Debug, "Thread %u exit background mode\n", Threads::CurrentThreadID().value);
                }

                idling_counter = 0u;
            } else if (!is_in_background_mode) {
                ++idling_counter;

                if (idling_counter > 10000) {
#ifdef HYP_WINDOWS
                    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#endif
                    is_in_background_mode = true;

                    DebugLog(LogType::Debug, "Thread %u begin background mode\n", Threads::CurrentThreadID().value);

                    idling_counter = 0u;
                }
            }
        }
    }

    std::atomic_bool m_is_running;
    UInt m_target_ticks_per_second;

    Queue<Scheduler::ScheduledTask> m_task_queue;
};

} // namespace hyperion::v2

#endif