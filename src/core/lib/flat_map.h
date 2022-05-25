#ifndef HYPERION_V2_LIB_FLAT_MAP_H
#define HYPERION_V2_LIB_FLAT_MAP_H

#include "flat_set.h"

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Key, class Value>
class FlatMap {
    using Pair = std::pair<Key, Value>;

    FlatSet<Pair> m_set;

public:
    using Iterator      = typename FlatSet<Pair>::Iterator;
    using ConstIterator = typename FlatSet<Pair>::ConstIterator;
    using InsertResult  = std::pair<Iterator, bool>; // iterator, was inserted

    FlatMap();
    FlatMap(std::initializer_list<Pair> initializer_list)
        : m_set(initializer_list)
    {
    }

    FlatMap(const FlatMap &other);
    FlatMap &operator=(const FlatMap &other);
    FlatMap(FlatMap &&other) noexcept;
    FlatMap &operator=(FlatMap &&other) noexcept;
    ~FlatMap();
    
    [[nodiscard]] Iterator Find(const Key &key);
    [[nodiscard]] ConstIterator Find(const Key &key) const;

    [[nodiscard]] bool Contains(const Key &key) const;

    InsertResult Insert(const Key &key, const Value &value);
    InsertResult Insert(const Key &key, Value &&value);

    template <class ...Args>
    InsertResult Emplace(const Key &key, Args &&... args)
        { return Insert(key, Value(std::forward<Args>(args)...)); }

    void Erase(Iterator iter);
    void Erase(const Key &key);

    [[nodiscard]] size_t Size() const { return m_set.Size(); }
    [[nodiscard]] Pair *Data() const  { return m_set.Data(); }
    [[nodiscard]] bool Empty() const  { return m_set.Empty(); }
    void Clear()                      { m_set.Clear(); }

    [[nodiscard]] Value &At(const Key &key)             { return Find(key)->second; }
    [[nodiscard]] const Value &At(const Key &key) const { return Find(key)->second; }

    [[nodiscard]] Value &operator[](const Key &key)
    {
        const auto iter = Insert(key, {}).first;

        return iter->second;
    }

    HYP_DEF_STL_ITERATOR(m_set)
};

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap() {}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(const FlatMap &other)
    : m_set(other.m_set)
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(const FlatMap &other) -> FlatMap &
{
    m_set = other.m_set;

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(FlatMap &&other) noexcept
    : m_set(std::move(other.m_set))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(FlatMap &&other) noexcept -> FlatMap&
{
    m_set = std::move(other.m_set);

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::~FlatMap() = default;

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) -> Iterator
{
    return std::lower_bound(
        m_set.begin(),
        m_set.end(),
        key,
        [](const Pair &lhs, const Key &rhs) {
            return lhs.first < rhs;
        }
    );
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    return std::lower_bound(
        m_set.cbegin(),
        m_set.cend(),
        key,
        [](const Pair &lhs, const Key &rhs) {
            return lhs.first < rhs;
        }
    );
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Contains(const Key &key) const
{
    return Find(key) != End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, const Value &value) -> InsertResult
{
    const auto iter = Find(key);

    if (iter == End() || iter->first != key) {
        return m_set.Insert(std::make_pair(key, value));
    }

    return {iter, false};
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, Value &&value) -> InsertResult
{
    const auto iter = Find(key);

    if (iter == End() || iter->first != key) {
        return m_set.Insert(std::make_pair(key, std::move(value)));
    }

    return {iter, false};
}

template <class Key, class Value>
void FlatMap<Key, Value>::Erase(Iterator iter)
{
    m_set.Erase(iter);
}

template <class Key, class Value>
void FlatMap<Key, Value>::Erase(const Key &value)
{
    const auto iter = Find(value);

    AssertThrow(iter != End());

    Erase(iter);
}

} // namespace hyperion

#endif