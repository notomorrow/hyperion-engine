#ifndef HYPERION_V2_THREAD_H
#define HYPERION_V2_THREAD_H

#include "lib/fixed_string.h"

#include <thread>
#include <tuple>

namespace hyperion::v2 {

template <class ...Args>
class Thread {
public:
    Thread(const FixedString &name);
    Thread(const Thread &other) = delete;
    Thread &operator=(const Thread &other) = delete;
    Thread(Thread &&other) noexcept;
    Thread &operator=(Thread &&other) noexcept;
    virtual ~Thread();

    bool Start(Args &&... args);
    bool Detach();
    bool Join();
    bool CanJoin() const;

protected:
    virtual void operator()(Args ...args) = 0;

private:
    FixedString m_name;
    std::thread *m_thread;
};

template <class ...Args>
Thread<Args...>::Thread(const FixedString &name)
    : m_name(name),
      m_thread(nullptr)
{
}

template <class ...Args>
Thread<Args...>::Thread(Thread &&other) noexcept
    : m_name(std::move(other.m_name)),
      m_thread(other.m_thread)
{
    other.m_thread = nullptr;
}

template <class ...Args>
Thread<Args...> &Thread<Args...>::operator=(Thread &&other) noexcept
{
    m_name = std::move(other.m_name);
    m_thread = other.m_thread;
    other.m_thread = nullptr;

    return *this;
}

template <class ...Args>
Thread<Args...>::~Thread()
{
    if (m_thread != nullptr) {
        if (m_thread->joinable()) {
            m_thread->join();
        }

        delete m_thread;
        m_thread = nullptr;
    }
}

template <class ...Args>
bool Thread<Args...>::Start(Args &&... args)
{
    if (m_thread != nullptr) {
        return false;
    }

    std::tuple<Args...> tuple_args(std::move(args)...);

    m_thread = new std::thread([&self = *this, tuple_args] {
        self(std::get<Args>(tuple_args)...);
    });

    return true;
}

template <class ...Args>
bool Thread<Args...>::Detach()
{
    if (m_thread == nullptr) {
        return false;
    }

    try {
        m_thread->detach();
    } catch (...) {
        return false;
    }

    return true;
}

template <class ...Args>
bool Thread<Args...>::Join()
{
    if (m_thread == nullptr) {
        return false;
    }
    
    try {
        m_thread->join();
    } catch (...) {
        return false;
    }

    return true;
}

template <class ...Args>
bool Thread<Args...>::CanJoin() const
{
    if (m_thread == nullptr) {
        return false;
    }

    return m_thread->joinable();
}

} // namespace hyperion::v2

#endif