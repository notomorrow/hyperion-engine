#ifndef HYPERION_V2_LIB_QUEUE_H
#define HYPERION_V2_LIB_QUEUE_H

#include "DynArray.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

/*! Queue based on DynArray<T>. About 30% faster than std::queue in my tests. */

template <class T>
class Queue : DynArray<T> {
public:
    using Iterator = typename DynArray<T>::Iterator;
    using ConstIterator = typename DynArray<T>::ConstIterator;

    Queue();
    Queue(const Queue &other);
    Queue(Queue &&other) noexcept;
    ~Queue();

    Queue &operator=(const Queue &other);
    Queue &operator=(Queue &&other) noexcept;

    [[nodiscard]] typename DynArray<T>::SizeType Size() const
        { return DynArray<T>::Size(); }

    [[nodiscard]] typename DynArray<T>::ValueType *Data()
        { return DynArray<T>::Data(); }

    [[nodiscard]] const typename DynArray<T>::ValueType *Data() const
        { return DynArray<T>::Data(); }

    [[nodiscard]] typename DynArray<T>::ValueType &Front()
        { return DynArray<T>::Front(); }

    [[nodiscard]] const typename DynArray<T>::ValueType &Front() const
        { return DynArray<T>::Front(); }

    [[nodiscard]] typename DynArray<T>::ValueType &Back()
        { return DynArray<T>::Back(); }

    [[nodiscard]] const typename DynArray<T>::ValueType &Back() const
        { return DynArray<T>::Back(); }

    [[nodiscard]] bool Empty() const
        { return DynArray<T>::Empty(); }

    [[nodiscard]] bool Any() const
        { return DynArray<T>::Any(); }

    [[nodiscard]] bool Contains(const T &value) const
        { return DynArray<T>::Contains(value); }

    void Reserve(typename DynArray<T>::SizeType capacity)
        { DynArray<T>::Reserve(capacity); }

    void Refit()
        { DynArray<T>::Refit(); }

    void Push(const typename DynArray<T>::ValueType &value);
    void Push(typename DynArray<T>::ValueType &&value);
    typename DynArray<T>::ValueType Pop();
    void Clear();

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_start_offset]),
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_size])
    )
};

template <class T>
Queue<T>::Queue()
    : DynArray<T>()
{
}

template <class T>
Queue<T>::Queue(const Queue &other)
    : DynArray<T>(other)
{
}

template <class T>
Queue<T>::Queue(Queue &&other) noexcept
    : DynArray<T>(std::move(other))
{
}

template <class T>
Queue<T>::~Queue()
{
}

template <class T>
auto Queue<T>::operator=(const Queue &other) -> Queue&
{
    DynArray<T>::operator=(other);

    return *this;
}

template <class T>
auto Queue<T>::operator=(Queue &&other) noexcept -> Queue&
{
    DynArray<T>::operator=(std::move(other));

    return *this;
}

template <class T>
void Queue<T>::Push(const typename DynArray<T>::ValueType &value)
{
    DynArray<T>::PushBack(value);
}

template <class T>
void Queue<T>::Push(typename DynArray<T>::ValueType &&value)
{
    DynArray<T>::PushBack(std::move(value));
}

template <class T>
auto Queue<T>::Pop() -> typename DynArray<T>::ValueType
{
    return DynArray<T>::PopFront();
}

template <class T>
void Queue<T>::Clear()
{
    DynArray<T>::Clear();
}

} // namespace hyperion

#endif