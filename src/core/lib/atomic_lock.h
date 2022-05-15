#ifndef HYPERION_V2_LIB_ATOMIC_LOCK_H
#define HYPERION_V2_LIB_ATOMIC_LOCK_H

#include <atomic>

namespace hyperion::v2 {

class AtomicLock {
public:
    AtomicLock() = default;
    AtomicLock(const AtomicLock &other) = delete;
    AtomicLock &operator=(const AtomicLock &other) = delete;
    AtomicLock(AtomicLock &&other) = delete;
    AtomicLock &operator=(AtomicLock &&other) = delete;
    ~AtomicLock() = default;

    void Lock()   { m_locked = true; }
    void Unlock() { m_locked = false; }
    void Wait() const
    {
        volatile uint32_t x = 0;

        while (m_locked) {
            ++x;
        }
    }

private:
    std::atomic_bool m_locked{false};
};

} // namespace hyperion::v2

#endif