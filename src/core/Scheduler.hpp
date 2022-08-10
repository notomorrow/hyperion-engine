#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

#define HYP_SCHEDULER_USE_ATOMIC_LOCK 0

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
#include "lib/AtomicLock.hpp"
#endif

#include "lib/AtomicSemaphore.hpp"
#include "lib/DynArray.hpp"

#include <Util.hpp>
#include <util/Defines.hpp>

#include <functional>
#include <atomic>
#include <thread>
#include <tuple>
#include <utility>
#include <deque>
#include <type_traits>

#if !HYP_SCHEDULER_USE_ATOMIC_LOCK
#include <mutex>
#include <condition_variable>
#endif

namespace hyperion::v2 {

struct ScheduledFunctionId {
    uint32_t value{0};
    
    ScheduledFunctionId &operator=(UInt id)
    {
        value = id;

        return *this;
    }
    
    ScheduledFunctionId &operator=(const ScheduledFunctionId &other) = default;

    bool operator==(uint32_t id) const                      { return value == id; }
    bool operator!=(uint32_t id) const                      { return value != id; }
    bool operator==(const ScheduledFunctionId &other) const { return value == other.value; }
    bool operator!=(const ScheduledFunctionId &other) const { return value != other.value; }
    bool operator<(const ScheduledFunctionId &other) const  { return value < other.value; }

    explicit operator bool() const { return value != 0; }
};

template <class ReturnType, class ...Args>
struct ScheduledFunction {
    using Function = std::function<ReturnType(Args...)>;

    ScheduledFunctionId id;
    Function            fn;

    constexpr static ScheduledFunctionId empty_id = ScheduledFunctionId{0};

    template <class Lambda>
    ScheduledFunction(Lambda &&lambda)
        : id {}
    {
        fn = std::forward<Lambda>(lambda);
    }

    // ScheduledFunction()
    //     : id {},
    //       fn(nullptr)
    // {
    // }

    ScheduledFunction(const ScheduledFunction &other) = default;
    ScheduledFunction &operator=(const ScheduledFunction &other) = default;
    ScheduledFunction(ScheduledFunction &&other) noexcept
        : id(other.id),
          fn(std::move(other.fn))
    {
        other.id = {};
        other.fn = nullptr;
    }

    ScheduledFunction &operator=(ScheduledFunction &&other) noexcept
    {
        id = other.id;
        fn = std::move(other.fn);

        other.id = {};
        other.fn = nullptr;

        return *this;
    }
    
    ~ScheduledFunction() = default;

    template <class ...Param>
    HYP_FORCE_INLINE ReturnType Execute(Param &&... args)
    {
        return fn(std::forward<Param>(args)...);
    }

    template <class ...Param>
    HYP_FORCE_INLINE ReturnType operator()(Param &&... args)
    {
        return Execute(std::forward<Param>(args)...);
    }
};

template <class ScheduledFunction>
class Scheduler {
    //using Executor          = std::add_pointer_t<void(typename ScheduledFunction::Function &)>;

    using ScheduledFunctionQueue = DynArray<ScheduledFunction>;
    using Iterator               = typename ScheduledFunctionQueue::Iterator;

public:
    using Task = ScheduledFunction;

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

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread. */
    ScheduledFunctionId Enqueue(ScheduledFunction &&fn)
    {
        std::unique_lock lock(m_mutex);

        return EnqueueInternal(std::forward<ScheduledFunction>(fn));
    }
    
    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    // bool Dequeue(ScheduledFunctionId id)
    // {
    //     if (id == ScheduledFunction::empty_id) {
    //         return false;
    //     }
        
    //     std::unique_lock lock(m_mutex);

    //     if (DequeueInternal(id)) {
    //         lock.unlock();

    //         if (m_scheduled_functions.Empty()) {
    //             m_is_flushed.notify_all();
    //         }

    //         return true;
    //     }

    //     return false;
    // }

    /*! If the enqueued with the given ID does _not_ exist, schedule the given function.
     *  else, remove the item with the given ID
     *  This is a helper function for create/destroy functions
     */
    // ScheduledFunctionId EnqueueReplace(ScheduledFunctionId &dequeue_id, ScheduledFunction &&enqueue_fn)
    // {
    //     std::unique_lock lock(m_mutex);

    //     if (dequeue_id != ScheduledFunction::empty_id) {
    //         auto it = Find(dequeue_id);

    //         if (it != m_scheduled_functions.End()) {
    //             (*it) = {
    //                 .id = {dequeue_id},
    //                 .fn = std::move(enqueue_fn)
    //             };

    //             return dequeue_id;
    //         }
    //     }

    //     return (dequeue_id = EnqueueInternal(std::forward<ScheduledFunction>(enqueue_fn)));
    // }

    /*! Wait for all tasks to be completed in another thread.
     * Must only be called from a different thread than the creation thread.
     */
    void AwaitExecution()
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
    template <class Executor>
    void FlushOrWait(Executor &&executor)
    {
        if (std::this_thread::get_id() == m_creation_thread) {
            Flush(std::forward<Executor>(executor));

            return;
        }

        std::unique_lock lock(m_mutex);
        m_is_flushed.wait(lock, [this] { return m_num_enqueued == 0; });
    }

    template <class Executor>
    void ExecuteFront(Executor &&executor)
    {
        AssertThrow(std::this_thread::get_id() == m_creation_thread);

        std::unique_lock lock(m_mutex);

        if (m_scheduled_functions.Any()) {
            executor(m_scheduled_functions.Front());

            m_scheduled_functions.PopFront();

            --m_num_enqueued;
        }

        lock.unlock();

        m_is_flushed.notify_all();
    }

    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptNext(Container &out_container)
    {
        AssertThrow(std::this_thread::get_id() == m_creation_thread);

        std::unique_lock lock(m_mutex);

        if (m_scheduled_functions.Any()) {
            out_container.Push(std::move(m_scheduled_functions.Front()));
            m_scheduled_functions.PopFront();

            --m_num_enqueued;
        }

        lock.unlock();

        m_is_flushed.notify_all();
    }
    
    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    template <class Executor>
    void Flush(Executor &&executor)
    {
        AssertThrow(std::this_thread::get_id() == m_creation_thread);

        std::unique_lock lock(m_mutex);

        while (m_scheduled_functions.Any()) {
            executor(m_scheduled_functions.Front());

            m_scheduled_functions.PopFront();
        }

        m_num_enqueued = 0;


        lock.unlock();

        m_is_flushed.notify_all();
    }
    
private:
    ScheduledFunctionId EnqueueInternal(ScheduledFunction &&fn)
    {
        fn.id = ++m_id_counter;

        m_scheduled_functions.PushBack(std::forward<ScheduledFunction>(fn));

        ++m_num_enqueued;

        return m_scheduled_functions.Back().id;
    }

    Iterator Find(ScheduledFunctionId id)
    {
        return m_scheduled_functions.FindIf([&id](const auto &item) {
            return item.id == id;
        });
    }

    // bool DequeueInternal(ScheduledFunctionId id)
    // {
    //     const auto it = Find(id);

    //     if (it == m_scheduled_functions.End()) {
    //         return false;
    //     }

    //     m_scheduled_functions.Erase(it);

    //     if (m_scheduled_functions.Empty()) {
    //         m_num_enqueued = 0;
    //     }

    //     return true;
    // }

    uint32_t                       m_id_counter = 0;
    std::atomic_uint32_t           m_num_enqueued{0};
    ScheduledFunctionQueue         m_scheduled_functions;

    std::mutex                     m_mutex;
    std::condition_variable        m_is_flushed;

    std::thread::id                m_creation_thread;
};

} // namespace hyperion::v2

#endif
