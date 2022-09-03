#ifndef HYPERION_V2_CORE_SCHEDULER_H
#define HYPERION_V2_CORE_SCHEDULER_H

#define HYP_SCHEDULER_USE_ATOMIC_LOCK 1

#include "lib/AtomicSemaphore.hpp"
#include "lib/DynArray.hpp"
#include "lib/FixedArray.hpp"
#include "lib/Proc.hpp"
#include <core/Thread.hpp>
#include <Threads.hpp>
#include <core/lib/Optional.hpp>

#include <Util.hpp>
#include <Types.hpp>
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

struct ScheduledFunctionID {
    UInt value { 0 };
    
    ScheduledFunctionID &operator=(UInt id)
    {
        value = id;

        return *this;
    }
    
    ScheduledFunctionID &operator=(const ScheduledFunctionID &other) = default;

    bool operator==(UInt id) const { return value == id; }
    bool operator!=(UInt id) const { return value != id; }
    bool operator==(const ScheduledFunctionID &other) const { return value == other.value; }
    bool operator!=(const ScheduledFunctionID &other) const { return value != other.value; }
    bool operator<(const ScheduledFunctionID &other) const { return value < other.value; }

    explicit operator bool() const { return value != 0; }
};

template <class ReturnType, class ...Args>
struct ScheduledFunction
{
    using Function = Proc<ReturnType, Args...>;//std::function<ReturnType(Args...)>;

    ScheduledFunctionID id;
    Function fn;

    constexpr static ScheduledFunctionID empty_id = ScheduledFunctionID { 0 };

    template <class Lambda>
    ScheduledFunction(Lambda &&lambda)
        : id { },
          fn(std::forward<Lambda>(lambda))
    {
    }

    ScheduledFunction(const ScheduledFunction &other) = delete;
    ScheduledFunction &operator=(const ScheduledFunction &other) = delete;
    ScheduledFunction(ScheduledFunction &&other) noexcept
        : id(other.id),
          fn(std::move(other.fn))
    {
        other.id = {};
        // other.fn = nullptr;
    }

    ScheduledFunction &operator=(ScheduledFunction &&other) noexcept
    {
        id = other.id;
        fn = std::move(other.fn);

        other.id = {};

        return *this;
    }
    
    ~ScheduledFunction() = default;
    
    HYP_FORCE_INLINE ReturnType Execute(Args... args)
    {
        return fn(std::forward<Args>(args)...);
    }
    
    HYP_FORCE_INLINE ReturnType operator()(Args... args)
    {
        return Execute(std::forward<Args>(args)...);
    }
};

template <class ScheduledFunction>
class Scheduler
{
    using ScheduledFunctionQueue = DynArray<ScheduledFunction>;
    using Iterator = typename ScheduledFunctionQueue::Iterator;

public:
    using Task = ScheduledFunction;
    using TaskID = ScheduledFunctionID;

    Scheduler()
        : m_owner_thread(Threads::CurrentThreadID())
    {
    }

    Scheduler(const Scheduler &other) = delete;
    Scheduler &operator=(const Scheduler &other) = delete;
    Scheduler(Scheduler &&other) = default;
    Scheduler &operator=(Scheduler &&other) = default;
    ~Scheduler() = default;

    HYP_FORCE_INLINE UInt NumEnqueued() const { return m_num_enqueued.load(); }

    /*! \brief Set the given thread ID to be the owner thread of this Scheduler.
     *  Tasks are to be enqueued from any other thread, and executed only from the owner thread.
     */
    void SetOwnerThread(ThreadID owner_thread)
    {
        m_owner_thread = owner_thread;
    }

    /*! \brief Enqueue a function to be executed on the owner thread. This is to be
     * called from a non-owner thread. */
    ScheduledFunctionID Enqueue(ScheduledFunction &&fn)
    {
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        auto result = EnqueueInternal(std::forward<ScheduledFunction>(fn));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#endif

        return result;
    }
    
    /*! \brief Remove a function from the owner thread's queue, if it exists
     * @returns a boolean value indicating whether or not the function was successfully dequeued */
    bool Dequeue(ScheduledFunctionID id)
    {
        if (id == ScheduledFunction::empty_id) {
            return false;
        }
        
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        if (DequeueInternal(id)) {
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
            m_sp.Signal();
#else
            if (m_scheduled_functions.Empty()) {
                lock.unlock();

                m_is_flushed.notify_all();
            }
#endif

            return true;
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#endif

        return false;
    }

    /*! \brief For each item, remove a function from the owner thread's queue, if it exists
     * @returns a FixedArray of booleans, each holding whether or not the corresponding function was successfully dequeued */
    template <SizeType Sz>
    FixedArray<bool, Sz> DequeueMany(const FixedArray<ScheduledFunctionID, Sz> &ids)
    {
        FixedArray<bool, Sz> results {};

        if (ids.Empty()) {
            return results;
        }
        
#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        for (SizeType i = 0; i < ids.Size(); i++) {
            const auto &id = ids[i];

            if (id == ScheduledFunction::empty_id) {
                results[i] = false;

                continue;
            }

            if (DequeueInternal(id)) {
                results[i] = true;
            }
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        if (m_scheduled_functions.Empty()) {
            lock.unlock();

            m_is_flushed.notify_all();
        }
#endif

        return results;
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
        if (Threads::IsOnThread(m_owner_thread)) {
            Flush(std::forward<Executor>(executor));

            return;
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        // TODO: do we need this?
        while (m_num_enqueued.load() != 0u);
#else
        std::unique_lock lock(m_mutex);
        m_is_flushed.wait(lock, [this] { return m_num_enqueued == 0; });
#endif
    }

    /* Move all the next pending task in the queue to an external container. */
    template <class Container>
    void AcceptNext(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        if (m_scheduled_functions.Any()) {
            out_container.Push(std::move(m_scheduled_functions.Front()));
            m_scheduled_functions.PopFront();

            --m_num_enqueued;
        }

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }
    
    /* Move all tasks in the queue to an external container. */
    template <class Container>
    void AcceptAll(Container &out_container)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        for (auto it = m_scheduled_functions.Begin(); it != m_scheduled_functions.End(); ++it) {
            out_container.Push(std::move(*it));
        }

        m_scheduled_functions.Clear();
        m_num_enqueued.store(0u);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }

    /*! Execute all scheduled tasks. May only be called from the creation thread.
     */
    template <class Executor>
    void Flush(Executor &&executor)
    {
        AssertThrow(Threads::IsOnThread(m_owner_thread));

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Wait();
#else
        std::unique_lock lock(m_mutex);
#endif

        while (m_scheduled_functions.Any()) {
            executor(m_scheduled_functions.Front());

            m_scheduled_functions.PopFront();
        }

        m_num_enqueued.store(0u);

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
        m_sp.Signal();
#else
        lock.unlock();

        m_is_flushed.notify_all();
#endif
    }
    
private:
    ScheduledFunctionID EnqueueInternal(ScheduledFunction &&fn)
    {
        fn.id = ++m_id_counter;

        m_scheduled_functions.PushBack(std::forward<ScheduledFunction>(fn));

        ++m_num_enqueued;

        return m_scheduled_functions.Back().id;
    }

    Iterator Find(ScheduledFunctionID id)
    {
        return m_scheduled_functions.FindIf([&id](const auto &item) {
            return item.id == id;
        });
    }

    bool DequeueInternal(ScheduledFunctionID id)
    {
        const auto it = Find(id);

        if (it == m_scheduled_functions.End()) {
            return false;
        }

        m_scheduled_functions.Erase(it);

        --m_num_enqueued;

        return true;
    }

    UInt m_id_counter = 0;
    std::atomic_uint m_num_enqueued { 0 };
    ScheduledFunctionQueue m_scheduled_functions;

#if HYP_SCHEDULER_USE_ATOMIC_LOCK
    BinarySemaphore m_sp;
#else
    std::mutex m_mutex;
    std::condition_variable m_is_flushed;
#endif

    ThreadID m_owner_thread;
};

} // namespace hyperion::v2

#endif
