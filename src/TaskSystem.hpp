#ifndef HYPERION_V2_TASK_SYSTEM_HPP
#define HYPERION_V2_TASK_SYSTEM_HPP

#include <core/lib/FixedArray.hpp>
#include <util/Defines.hpp>

#include "Threads.hpp"
#include "TaskThread.hpp"

#include <Types.hpp>

#include <atomic>

#define HYP_NUM_TASK_THREADS_4
//#define HYP_NUM_TASK_THREADS_8

namespace hyperion::v2 {

struct TaskRef
{
    TaskThread *runner;
    TaskThread::Scheduler::TaskID id;
};

struct TaskBatch
{
    std::atomic<UInt> num_completed;
    UInt num_enqueued;

    /* Number of tasks must remain constant from creation of the TaskBatch,
     * to completion. */
    DynArray<TaskThread::Scheduler::Task> tasks;

    /* TaskRefs to be set by the TaskSystem, holding task ids and pointers to the threads
     * each task has been scheduled to. */
    DynArray<TaskRef> task_refs;

    /*! \brief Add a task to be ran with this batch. Note: adding a task while the batch is already running
     * does not mean the newly added task will be ran! You'll need to re-enqueue the batch after the previous one has been completed.
     */
    void AddTask(TaskThread::Scheduler::Task &&task)
    {
        tasks.PushBack(std::move(task));
    }

    bool IsCompleted() const
    {
        return num_completed.load(std::memory_order_relaxed) >= num_enqueued;
    }

    /*! \brief Block the current thread until all tasks have been marked as completed. */
    void AwaitCompletion() const
    {
        while (!IsCompleted()) {
            HYP_WAIT_IDLE();
        }
    }

    /*! \brief Execute each non-enqueued task in serial (not async). */
    void ForceExecute()
    {
        for (auto &task : tasks) {
            task.Execute();
        }

        tasks.Clear();
    }
};

class TaskSystem
{
    static constexpr UInt target_ticks_per_second = 0;

public:
    TaskSystem()
        : m_cycle { 0u },
          m_task_thread_0(Threads::thread_ids.At(THREAD_TASK_0), target_ticks_per_second),
          m_task_thread_1(Threads::thread_ids.At(THREAD_TASK_1), target_ticks_per_second),
#if defined(HYP_NUM_TASK_THREADS_4) || defined(HYP_NUM_TASK_THREADS_8)
          m_task_thread_2(Threads::thread_ids.At(THREAD_TASK_2), target_ticks_per_second),
          m_task_thread_3(Threads::thread_ids.At(THREAD_TASK_3), target_ticks_per_second),
#endif
#ifdef HYP_NUM_TASK_THREADS_8
          m_task_thread_4(Threads::thread_ids.At(THREAD_TASK_4), target_ticks_per_second),
          m_task_thread_5(Threads::thread_ids.At(THREAD_TASK_5), target_ticks_per_second),
          m_task_thread_6(Threads::thread_ids.At(THREAD_TASK_6), target_ticks_per_second),
          m_task_thread_7(Threads::thread_ids.At(THREAD_TASK_7), target_ticks_per_second),
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

    /*! \brief Enqueue a batch of multiple Tasks. Each Task will be enqueued to run in parallel.
     * You will need to call AwaitCompletion() before the pointer to task batch is destroyed.
     */
    TaskBatch *EnqueueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);
        batch->num_completed.store(0u, std::memory_order_relaxed);
        batch->num_enqueued = 0u;

        batch->task_refs.Resize(batch->tasks.Size());

        for (SizeType i = 0; i < batch->tasks.Size(); i++) {
            auto &task = batch->tasks[i];
            const auto cycle = m_cycle.load(std::memory_order_relaxed);
            auto *task_thread = m_task_threads[cycle];
            
            const auto task_id = task_thread->ScheduleTask(std::move(task), &batch->num_completed);

            ++batch->num_enqueued;

            batch->task_refs[i] = TaskRef {
                task_thread,
                task_id
            };

            m_cycle.store((cycle + 1) % m_task_threads.Size(), std::memory_order_relaxed);
        }

        // all have been moved
        batch->tasks.Clear();

        return batch;
    }

    /*! \brief Dequeue each task in a TaskBatch. A potentially expensive operation,
     * as each task will have to individually be dequeued, performing a lock operation.
     * @param batch Pointer to the TaskBatch to dequeue
     * @returns A DynArray<bool> containing for each Task that has been enqueued, whether or not
     * it was successfully dequeued.
     */
    DynArray<bool> DequeueBatch(TaskBatch *batch)
    {
        AssertThrow(batch != nullptr);

        DynArray<bool> results;
        results.Resize(batch->task_refs.Size());

        for (SizeType i = 0; i < batch->task_refs.Size(); i++) {
            auto &task_ref = batch->task_refs[i];

            if (task_ref.runner == nullptr) {
                continue;
            }

            results[i] = task_ref.runner->GetScheduler().Dequeue(task_ref.id);
        }

        return results;
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

    DynArray<TaskBatch *> m_running_batches;
};

} // namespace hyperion::v2

#endif