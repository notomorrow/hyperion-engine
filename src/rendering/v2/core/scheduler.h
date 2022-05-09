#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

#include <util.h>

#include <functional>
#include <atomic>
#include <mutex>
#include <tuple>
#include <utility>
#include <deque>

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
public:
    Scheduler() = default;
    Scheduler(const Scheduler &other) = delete;
    Scheduler &operator=(const Scheduler &other) = delete;
    Scheduler(Scheduler &&other) = default;
    Scheduler &operator=(Scheduler &&other) = default;
    ~Scheduler() = default;

    HYP_FORCE_INLINE bool HasEnqueued() const { return m_has_messages.load(); }
    
    typename ScheduledFunction::ID Enqueue(typename ScheduledFunction::Function &&fn)
    {
        std::lock_guard guard(m_mutex);

        return EnqueueInternal(std::move(fn));
    }

    void Dequeue(typename ScheduledFunction::ID id)
    {
        if (id ==  ScheduledFunction::empty_id) {
            return;
        }

        std::lock_guard guard(m_mutex);

        DequeueInternal(id);
    }

    void WaitForFlush()
    {
        std::cout << "WAIT FOR FLUSH\n";
        std::unique_lock lock(m_mutex);
        m_condition.wait(lock, [this] { return !m_has_messages; });
    }

    /*! If the enqueued with the given ID does _not_ exist, schedule the given function.
     *  else, remove the item with the given ID
     *  This is a helper function for create/destroy functions
     */
    typename ScheduledFunction::ID DequeueOrEnqueue(typename ScheduledFunction::ID dequeue_id, typename ScheduledFunction::Function &&enqueue_fn)
    {
        std::lock_guard guard(m_mutex);

        if (dequeue_id == ScheduledFunction::empty_id || DequeueInternal(dequeue_id)) {
            return ScheduledFunction::empty_id;
        }

        return EnqueueInternal(std::move(enqueue_fn));
    }

    void Flush(Args &&... args)
    {
        std::cout << "FLUSH\n";
        std::unique_lock lock(m_mutex);

        while (!m_scheduled_functions.empty()) {
            //std::apply(std::move(m_scheduled_functions.front().fn), std::move(m_scheduled_functions.front().args));
            m_scheduled_functions.front().fn(std::forward<Args>(args)...);

            m_scheduled_functions.pop_front();
        }

        m_has_messages = false;

        lock.unlock();

        m_condition.notify_all();
    }
    
private:
    typename ScheduledFunction::ID EnqueueInternal(typename ScheduledFunction::Function &&fn)
    {
        m_scheduled_functions.push_back({
            .id = {{++m_id_counter}},
            .fn = std::move(fn)
        });

        m_has_messages = true;

        return m_scheduled_functions.back().id;
    }

    bool DequeueInternal(typename ScheduledFunction::ID id)
    {
        const auto it = std::find_if(
            m_scheduled_functions.begin(),
            m_scheduled_functions.end(),
            [&id](const auto &item) {
                return item.id == id;
            }
        );

        if (it == m_scheduled_functions.end()) {
            return false;
        }

        m_scheduled_functions.erase(it);

        if (m_scheduled_functions.empty()) {
            m_has_messages = false;
        }

        return true;
    }

    uint32_t                       m_id_counter = 0;
    std::atomic_bool               m_has_messages;
    std::mutex                     m_mutex;
    std::deque<ScheduledFunction>  m_scheduled_functions;
    std::condition_variable        m_condition;
};

} // namespace hyperion::v2

#endif