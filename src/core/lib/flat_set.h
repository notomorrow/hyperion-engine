#ifndef HYPERION_V2_LIB_FLAT_SET_H
#define HYPERION_V2_LIB_FLAT_SET_H

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class T>
class FlatSet {
    std::vector<T> m_vector;

public:
    using Iterator      = typename std::vector<T>::iterator;
    using ConstIterator = typename std::vector<T>::const_iterator;
    using InsertResult  = std::pair<Iterator, bool>; // iterator, was inserted

    FlatSet();
    FlatSet(std::initializer_list<T> initializer_list)
        : m_vector(initializer_list)
    {
    }

    FlatSet(const FlatSet &other);
    FlatSet &operator=(const FlatSet &other);
    FlatSet(FlatSet &&other) noexcept;
    FlatSet &operator=(FlatSet &&other) noexcept;
    ~FlatSet();
    
    [[nodiscard]] Iterator Find(const T &value);
    [[nodiscard]] ConstIterator Find(const T &value) const;

    InsertResult Insert(const T &value);
    InsertResult Insert(T &&value);

    template <class ...Args>
    InsertResult Emplace(Args &&... args)
        { return Insert(T(std::forward<Args>(args)...)); }

    void Erase(Iterator iter);
    void Erase(const T &value);

    [[nodiscard]] size_t Size() const                     { return m_vector.size(); }
    [[nodiscard]] T *Data() const                         { return m_vector.data(); }
    [[nodiscard]] bool Empty() const                      { return m_vector.empty(); }
    void Clear()                                          { m_vector.clear(); }

    [[nodiscard]] T &At(size_t index)                     { return m_vector.at(index); }
    [[nodiscard]] const T &At(size_t index) const         { return m_vector.at(index); }

    [[nodiscard]] T &operator[](size_t index)             { return m_vector[index]; }
    [[nodiscard]] const T &operator[](size_t index) const { return m_vector[index]; }

    HYP_DEF_STL_ITERATOR(m_vector)

private:
    Iterator GetIterator(const T &value)
    {
        return std::lower_bound(
            m_vector.begin(),
            m_vector.end(),
            value
        );
    }

    ConstIterator GetIterator(const T &value) const
    {
        return std::lower_bound(
            m_vector.cbegin(),
            m_vector.cend(),
            value
        );
    }
};

template <class T>
FlatSet<T>::FlatSet() {}

template <class T>
FlatSet<T>::FlatSet(const FlatSet &other)
    : m_vector(other.m_vector)
{
}

template <class T>
auto FlatSet<T>::operator=(const FlatSet &other) -> FlatSet&
{
    m_vector = other.m_vector;

    return *this;
}

template <class T>
FlatSet<T>::FlatSet(FlatSet &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class T>
auto FlatSet<T>::operator=(FlatSet &&other) noexcept -> FlatSet&
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class T>
FlatSet<T>::~FlatSet() = default;

template <class T>
auto FlatSet<T>::Find(const T &value) -> Iterator
{
    return GetIterator(value);
}

template <class T>
auto FlatSet<T>::Find(const T &value) const -> ConstIterator
{
    return GetIterator(value);
}

template <class T>
auto FlatSet<T>::Insert(const T &value) -> InsertResult
{
    Iterator it = GetIterator(value);

    if (it == End() || *it != value) {
        it = m_vector.insert(it, value);

        return {it, true};
    }

    return {it, false};
}

template <class T>
auto FlatSet<T>::Insert(T &&value) -> InsertResult
{
    Iterator it = GetIterator(value);

    if (it == End() || *it != value) {
        it = m_vector.insert(it, std::forward<T>(value));

        return {it, true};
    }

    return {it, false};
}

template <class T>
void FlatSet<T>::Erase(Iterator iter)
{
    m_vector.erase(iter);
}

template <class T>
void FlatSet<T>::Erase(const T &value)
{
    const auto iter = Find(value);

    AssertThrow(iter != End());

    Erase(iter);
}

} // namespace hyperion

#endif