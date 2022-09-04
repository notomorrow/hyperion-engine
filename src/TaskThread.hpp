#ifndef HYPERION_V2_TASK_THREAD_H
#define HYPERION_V2_TASK_THREAD_H

#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/lib/Queue.hpp>
#include <math/MathUtil.hpp>
#include <GameCounter.hpp>

#include <Types.hpp>

#include <atomic>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

struct ThreadID;

class TaskThread : public Thread<Scheduler<ScheduledFunction<void>>>
{
public:
    TaskThread(const ThreadID &thread_id, UInt target_ticks_per_second = 0)
        : Thread(thread_id),
          m_is_running(false),
          m_target_ticks_per_second(target_ticks_per_second)
    {
    }

    virtual ~TaskThread() = default;

    bool IsRunning() const { return m_is_running; }
    void Stop() { m_is_running = false; }

protected:
    virtual void operator()() override
    {
        m_is_running = true;

        const bool is_locked = m_target_ticks_per_second != 0;
        LockstepGameCounter counter(is_locked ? (1.0f / static_cast<Float>(m_target_ticks_per_second)) : 1.0f);

        while (m_is_running) {
            if (is_locked) {
                while (counter.Waiting()) {
                    /* wait */
                }

                counter.NextTick();
            }

            if (m_scheduler.NumEnqueued()) {
                /*m_scheduler.AcceptAll(m_task_queue);

                // do not execute within lock
                while (m_task_queue.Any()) {
                    m_task_queue.Front().Execute();

                    m_task_queue.Pop();
                }*/

                // TEMP: Testing for atomic counter inc. Will have to move back
                m_scheduler.Flush([](auto &task) {
                    task();
                });
            }
        }
    }

    std::atomic_bool m_is_running;
    UInt m_target_ticks_per_second;

    Queue<typename Scheduler::Task> m_task_queue;
};

} // namespace hyperion::v2

#endif