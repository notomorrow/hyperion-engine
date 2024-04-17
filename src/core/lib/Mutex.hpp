/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIB_MUTEX_HPP
#define HYPERION_LIB_MUTEX_HPP

#include <mutex>
namespace hyperion {

// @TODO: Implement without using STL mutex, so we can avoid ABI breakage

class Mutex
{
public:
    struct Guard
    {
        Guard(Mutex &mutex)
            : mutex(mutex)
        {
            mutex.Lock();
        }

        Guard(const Guard &other)                   = delete;
        Guard &operator=(const Guard &other)        = delete;
        Guard(Guard &&other) noexcept               = delete;
        Guard &operator=(Guard &&other) noexcept    = delete;

        ~Guard()
        {
            mutex.Unlock();
        }

        Mutex &mutex;
    };

    Mutex()                                     = default;
    Mutex(const Mutex &other)                   = delete;
    Mutex &operator=(const Mutex &other)        = delete;
    Mutex(Mutex &&other) noexcept               = delete;
    Mutex &operator=(Mutex &&other) noexcept    = delete;
    ~Mutex()                                    = default;

    void Lock()
        { m_mutex.lock(); }

    void Unlock()
        { m_mutex.unlock(); }

private:
    std::mutex m_mutex;
};

} // namespace hyperion

#endif