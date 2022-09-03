#ifndef HYPERION_V2_TASK_SYSTEM_HPP
#define HYPERION_V2_TASK_SYSTEM_HPP

#include <core/lib/FixedArray.hpp>

#include "Threads.hpp"
#include "TaskThread.hpp"

#include <Types.hpp>

#include <atomic>

#define HYP_NUM_TASK_THREADS_4
//#define HYP_NUM_TASK_THREADS_8

namespace hyperion::v2 {

class TaskSystem
{
    static constexpr Float target_ticks_per_second = 60.0f;

public:
    struct TaskRef {
        TaskThread *runner;
        TaskThread::Scheduler::TaskID id;
    };

    TaskSystem()
        : m_cycle { 0u },
          m_task_thread_0(TaskThread(Threads::thread_ids.At(THREAD_TASK_0), target_ticks_per_second)),
          m_task_thread_1(TaskThread(Threads::thread_ids.At(THREAD_TASK_1), target_ticks_per_second)),
#if defined(HYP_NUM_TASK_THREADS_4) || defined(HYP_NUM_TASK_THREADS_8)
          m_task_thread_2(TaskThread(Threads::thread_ids.At(THREAD_TASK_2), target_ticks_per_second)),
          m_task_thread_3(TaskThread(Threads::thread_ids.At(THREAD_TASK_3), target_ticks_per_second)),
#endif
#ifdef HYP_NUM_TASK_THREADS_8
          m_task_thread_4(TaskThread(Threads::thread_ids.At(THREAD_TASK_4), target_ticks_per_second)),
          m_task_thread_5(TaskThread(Threads::thread_ids.At(THREAD_TASK_5), target_ticks_per_second)),
          m_task_thread_6(TaskThread(Threads::thread_ids.At(THREAD_TASK_6), target_ticks_per_second)),
          m_task_thread_7(TaskThread(Threads::thread_ids.At(THREAD_TASK_7), target_ticks_per_second)),
#endif
          m_task_threads {
              &m_task_thread_0,
              &m_task_thread_1,
#if defined(HYP_NUM_TASK_THREADS_4) || defined(HYP_NUM_TASK_THREADS_8)
              &m_task_thread_2,
              &m_task_thread_3,
#endif
#ifdef HYP_NUM_TASK_THREADS_8
              &m_task_thread_4,
              &m_task_thread_5,
              &m_task_thread_6,
              &m_task_thread_7
#endif
          }
    {
    }

    TaskSystem(const TaskSystem &other) = delete;
    TaskSystem &operator=(const TaskSystem &other) = delete;

    TaskSystem(TaskSystem &&other) noexcept = delete;
    TaskSystem &operator=(TaskSystem &&other) noexcept = delete;

    ~TaskSystem() = default;

    void Start()
    {
        for (auto &it : m_task_threads) {
            AssertThrow(it->Start());
        }
    }

    void Stop()
    {
        for (auto &it : m_task_threads) {
            it->Stop();
            it->Join();
        }
    }

    template <class Task>
    TaskRef ScheduleTask(Task &&task)
    {
        const auto cycle = m_cycle.load();

        TaskThread *task_thread = m_task_threads[cycle];

        const auto task_id = task_thread->ScheduleTask(std::forward<Task>(task));

        m_cycle.store((cycle + 1) % m_task_threads.Size());

        return TaskRef {
            task_thread,
            task_id
        };
    }

    bool Unschedule(const TaskRef &task_ref)
    {
        return task_ref.runner->GetScheduler().Dequeue(task_ref.id);
    }

private:
    std::atomic_uint m_cycle;
    TaskThread m_task_thread_0;
    TaskThread m_task_thread_1;
#if defined(HYP_NUM_TASK_THREADS_4) || defined(HYP_NUM_TASK_THREADS_8)
    TaskThread m_task_thread_2;
    TaskThread m_task_thread_3;
#endif
#ifdef HYP_NUM_TASK_THREADS_8
    TaskThread m_task_thread_4;
    TaskThread m_task_thread_5;
    TaskThread m_task_thread_6;
    TaskThread m_task_thread_7;
#endif

#if defined(HYP_NUM_TASK_THREADS_8)
    FixedArray<TaskThread *, 8> m_task_threads;
#elif defined(HYP_NUM_TASK_THREADS_4)
    FixedArray<TaskThread *, 4> m_task_threads;
#endif
};

} // namespace hyperion::v2

#endif