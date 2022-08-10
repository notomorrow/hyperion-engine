#ifndef HYPERION_V2_LIB_SORTED_ARRAY_H
#define HYPERION_V2_LIB_SORTED_ARRAY_H

#include "ContainerBase.hpp"
#include "DynArray.hpp"
#include "Pair.hpp"
#include <util/Defines.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class T>
class SortedArray : public DynArray<T> {
public:
    using Iterator      = typename DynArray<T>::Iterator;
    using ConstIterator = typename DynArray<T>::ConstIterator;

    SortedArray();

    SortedArray(std::initializer_list<T> initializer_list)
        : DynArray<T>(initializer_list)
    {
    }

    SortedArray(const SortedArray &other);
    SortedArray &operator=(const SortedArray &other);
    SortedArray(SortedArray &&other) noexcept;
    SortedArray &operator=(SortedArray &&other) noexcept;
    ~SortedArray();
    
    [[nodiscard]] Iterator Find(const T &value);
    [[nodiscard]] ConstIterator Find(const T &value) const;
    
    Iterator Insert(const T &value);
    Iterator Insert(T &&value);

    //! \brief Performs a direct call to DynArray::Erase(), erasing the element at the iterator
    //  position.
    Iterator Erase(ConstIterator it)                      { return DynArray<T>::Erase(it); }
    //! \brief Erase an element by value. The item is searched for using binary search,
    //  and if the item was found, it will be erased (and iterators will be invalidated)
    Iterator Erase(const T &value);

    [[nodiscard]] size_t Size() const                     { return DynArray<T>::Size(); }
    [[nodiscard]] T *Data()                               { return DynArray<T>::Data(); }
    [[nodiscard]] T * const Data() const                  { return DynArray<T>::Data(); }
    [[nodiscard]] bool Empty() const                      { return DynArray<T>::Empty(); }
    [[nodiscard]] bool Any() const                        { return DynArray<T>::Any(); }
    [[nodiscard]] bool Contains(const T &value) const     { return Find(value) != End(); }
    void Clear()                                          { DynArray<T>::Clear(); }
    
    [[nodiscard]] T &Front()                              { return DynArray<T>::Front(); }
    [[nodiscard]] const T &Front() const                  { return DynArray<T>::Front(); }
    [[nodiscard]] T &Back()                               { return DynArray<T>::Back(); }
    [[nodiscard]] const T &Back() const                   { return DynArray<T>::Back(); }

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_start_offset]),
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_size])
    )
};

template <class T>
SortedArray<T>::SortedArray()
    : DynArray<T>()
{
    
}

template <class T>
SortedArray<T>::SortedArray(const SortedArray &other)
    : DynArray<T>(other)
{
}

template <class T>
auto SortedArray<T>::operator=(const SortedArray &other) -> SortedArray&
{
    DynArray<T>::operator=(other);

    return *this;
}

template <class T>
SortedArray<T>::SortedArray(SortedArray &&other) noexcept
    : DynArray<T>(std::move(other))
{
}

template <class T>
auto SortedArray<T>::operator=(SortedArray &&other) noexcept -> SortedArray&
{
    DynArray<T>::operator=(std::move(other));

    return *this;
}

template <class T>
SortedArray<T>::~SortedArray() = default;

template <class T>
auto SortedArray<T>::Find(const T &value) -> Iterator
{
    const auto it = SortedArray<T>::Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Find(const T &value) const -> ConstIterator
{
    const auto it = SortedArray<T>::Base::LowerBound(value);

    if (it == End()) {
        return it;
    }

    return (*it == value) ? it : End();
}

template <class T>
auto SortedArray<T>::Insert(const T &value) -> Iterator
{
    Iterator it = SortedArray<T>::Base::LowerBound(value);

    return DynArray<T>::Insert(it, value);
}

template <class T>
auto SortedArray<T>::Insert(T &&value) -> Iterator
{
    Iterator it = SortedArray<T>::Base::LowerBound(value);

    return DynArray<T>::Insert(it, std::forward<T>(value));
}

template <class T>
auto SortedArray<T>::Erase(const T &value) -> Iterator
{
    const ConstIterator iter = Find(value);

    if (iter == End()) {
        return End();
    }

    return DynArray<T>::Erase(iter);
}

} // namespace hyperion

#endif
