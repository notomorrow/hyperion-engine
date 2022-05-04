#ifndef HYPERION_SMALL_VECTOR_H
#define HYPERION_SMALL_VECTOR_H


#if 0
#include <util.h>

namespace hyperion {

template <class T, size_t ReservedSize = 4096 / sizeof(T)>
class SmallVector
{
public:
    SmallVector() = default;
    ~SmallVector()
    {
        if (m_dynamic_storage != nullptr) {
            delete[] m_dynamic_storage;
        }
    }

    HYP_FORCE_INLINE T &operator[](size_t index)             { return m_head[index]; }
    HYP_FORCE_INLINE const T &operator[](size_t index) const { return m_head[index]; }

    HYP_FORCE_INLINE T *Data() const { return m_head; }
    HYP_FORCE_INLINE size_t Size() const { return m_size; }

    void PushBack(const T &item)
    {
        size_t new_size = m_size + 1;

        if (m_head == &m_reserved_storage[0] && new_size >= ReservedSize) {
            AssertThrow(m_dynamic_storage == nullptr);
            /* TODO: placement new / MOVE ? */
            m_dynamic_storage = new T[new_size * 2];
            std::memcpy(&m_dynamic_storage[0], &m_reserved_storage[0], sizeof(T) * m_size);

            m_head = m_dynamic_storage;
        }

        m_head[new_size] = item;
    }

private:
    T m_reserved_storage[ReservedSize];
    T *m_dynamic_storage = nullptr;
    T *m_head            = &m_reserved_storage[0];
    size_t m_size        = 0;
};

} // namespace hyperion

#endif

#endif