#ifndef HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H
#define HYPERION_V2_LIB_ATOMIC_SEMAPHORE_H

#include <atomic>

namespace hyperion::v2 {

/* waits for zero to be accessing */
template <class T = int>
class AtomicSemaphore {
public:
    AtomicSemaphore() = default;
    AtomicSemaphore(const AtomicSemaphore &other) = delete;
    AtomicSemaphore &operator=(const AtomicSemaphore &other) = delete;
    AtomicSemaphore(AtomicSemaphore &&other) = delete;
    AtomicSemaphore &operator=(AtomicSemaphore &&other) = delete;
    ~AtomicSemaphore() = default;

    void Inc() { ++m_count; }
    void Dec() { --m_count; }

    T Count() const { return m_count.load(); }

    void WaitUntilValue(T value) const
    {
        volatile uint32_t x = 0;

        while (m_count != value) {
            ++x;
        }
    }

private:
    std::atomic<T> m_count{0};
};

} // namespace hyperion::v2

#endif