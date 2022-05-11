#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

#include <util.h>

#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <deque>
#include <condition_variable>

namespace hyperion::v2 {

template <class ReturnType, class ...Args>
struct ScheduledFunction {
    using Function = std::function<ReturnType(Args...)>;

    struct ID {
        uint32_t value;

        bool operator==(const ID &other) const { return value == other.value; }
    } id;

    Function fn;

    constexpr static ID empty_id = ID{0};
};

template <class ReturnType, class ...Args>
class Scheduler {
    using ScheduledFunction = ScheduledFunction<ReturnType, Args...>;
    using Executor          = std::add_pointer_t<void(typename ScheduledFunction::Function &)>;

    using ScheduledFunctionQueue = std::deque<ScheduledFunction>;
    using Iterator               = typename ScheduledFunctionQueue::iterator;

public:
    Scheduler()
        : m_creation_thread(std::this_thread::get_id())
    {
    }

    Scheduler(const Scheduler &other) = delete;
    Scheduler &operator=(const Scheduler &other) = delete;
    Scheduler(Scheduler &&other) = default;
    Scheduler &operator=(Scheduler &&other) = default;
    ~Scheduler() = default;

    HYP_FORCE_INLINE uint32_t NumEnqueued() const { return m_num_enqueued.load(); }
    
    typename ScheduledFunction::ID Enqueue(typename ScheduledFunction::Function &&fn)
    {
        std::lock_guard guard(m_mutex);

        return EnqueueInternal(std::move(fn));
    }

    void Dequeue(typename ScheduledFunction::ID id)
    {
        if (id == ScheduledFunction::empty_id) {
            return;
        }

        std::unique_lock lock(m_mutex);

        if (DequeueInternal(id)) {
            lock.unlock();

            if (m_scheduled_functions.empty()) {
                m_is_flushed.notify_all();
            }
        }
    }

    /*! If the enqueued with the given ID does _not_ exist, schedule the given function.
     *  else, remove the item with the given ID
     *  This is a helper function for create/destroy functions
     */
    typename ScheduledFunction::ID Replace(typename ScheduledFunction::ID dequeue_id, typename ScheduledFunction::Function &&enqueue_fn)
    {
        std::unique_lock lock(m_mutex);

        if (dequeue_id != ScheduledFunction::empty_id) {
            auto it = Find(dequeue_id);

            if (it != m_scheduled_functions.end()) {
                (*it) = {
                    .id = {dequeue_id},
                    .fn = std::move(enqueue_fn)
                };

                return dequeue_id;
            }
        }

        return EnqueueInternal(std::move(enqueue_fn));
    }
    
    /*! Wait for all tasks to be completed in another thread.
     * Must only be called from a different thread than the creation thread.
     */
    void Wait()
    {
        AssertThrow(std::this_thread::get_id() != m_creation_thread);

        std::unique_lock lock(m_mutex);
        m_is_flushed.wait(lock, [this] { return m_num_enqueued == 0; });
    }

    /*! If the current thread is the creation thread, the scheduler will be flushed and immediately return.
     * Otherwise, the thread will block until all tasks have been executed.
     */
    HYP_FORCE_INLINE void FlushOrWait()
    {
        FlushOrWait([](auto &fn) { fn(); });
    }

    /*! If the current thread is the creation thread, the scheduler will be flushed and immediately return.
     * Otherwise, the thread will block until all tasks have been executed.
     */
    void FlushOrWait(Executor &&executor)
    {
        if (std::this_thread::get_id() == m_creation_thread) {
            Flush(std::move(executor));

            return;
        }

        std::unique_lock lock(m_mutex);
        m_is_flushed.wait(lock, [this] { return m_num_enqueued == 0; });
    }

    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    HYP_FORCE_INLINE void Flush()
    {
        Flush([](auto &fn) { fn(); });
    }
    
    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    void Flush(Executor &&executor)
    {
        AssertThrow(std::this_thread::get_id() == m_creation_thread);
        
        std::unique_lock lock(m_mutex);

        while (!m_scheduled_functions.empty()) {
            executor(m_scheduled_functions.front().fn);

            m_scheduled_functions.pop_front();
        }

        m_num_enqueued = 0;

        lock.unlock();

        m_is_flushed.notify_all();
    }
    
private:
    typename ScheduledFunction::ID EnqueueInternal(typename ScheduledFunction::Function &&fn)
    {
        m_scheduled_functions.push_back({
            .id = {{++m_id_counter}},
            .fn = std::move(fn)
        });

        ++m_num_enqueued;

        return m_scheduled_functions.back().id;
    }

    Iterator Find(typename ScheduledFunction::ID id)
    {
        return std::find_if(
            m_scheduled_functions.begin(),
            m_scheduled_functions.end(),
            [&id](const auto &item) {
                return item.id == id;
            }
        );
    }

    bool DequeueInternal(typename ScheduledFunction::ID id)
    {
        const auto it = Find(it);

        if (it == m_scheduled_functions.end()) {
            return false;
        }

        m_scheduled_functions.erase(it);

        if (m_scheduled_functions.empty()) {
            m_num_enqueued = 0;
        }

        return true;
    }

    uint32_t                       m_id_counter = 0;
    std::atomic_uint32_t           m_num_enqueued{0};
    std::mutex                     m_mutex;
    ScheduledFunctionQueue         m_scheduled_functions;
    std::condition_variable        m_is_flushed;

    std::thread::id                m_creation_thread;
};

} // namespace hyperion::v2

#endif