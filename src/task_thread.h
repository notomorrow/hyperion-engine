#ifndef HYPERION_V2_TASK_THREAD_H
#define HYPERION_V2_TASK_THREAD_H

#include <core/thread.h>
#include <core/scheduler.h>
#include <core/containers.h>

#include <atomic>

namespace hyperion {

class SystemWindow;

} // namespace hyperion

namespace hyperion::v2 {

struct ThreadId;

class TaskThread : public Thread<Scheduler<void>>
{
public:
    TaskThread(const ThreadId &thread_id)
        : Thread(thread_id),
          m_is_running(false)
    {
    }

    virtual ~TaskThread() = default;

    bool IsRunning() const { return m_is_running; }
    void Stop()            { m_is_running = false; }

protected:
    virtual void operator()() override
    {
        m_is_running = true;

        while (m_is_running) {
            if (auto num_enqueued = m_scheduler->NumEnqueued()) {
                m_scheduler->Flush([](auto &fn) {
                    fn();
                });
            }
        }
    }

    std::atomic_bool m_is_running;
};

} // namespace hyperion::v2

#endif