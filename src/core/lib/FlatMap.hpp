#ifndef HYPERION_V2_LIB_FLAT_MAP_H
#define HYPERION_V2_LIB_FLAT_MAP_H

#include "FlatSet.hpp"
#include "DynArray.hpp"
#include "Pair.hpp"
#include "ContainerBase.hpp"
#include <HashCode.hpp>

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Key, class Value>
class FlatMap : public ContainerBase<FlatMap<Key, Value>, Key>
{
public:
    using KeyValuePair = KeyValuePair<Key, Value>;

private:
    DynArray<KeyValuePair> m_vector;

public:
    using Base = ContainerBase<FlatMap<Key, Value>, Key>;

    using KeyType = Key;
    using ValueType = Value;

    using Iterator = typename decltype(m_vector)::Iterator;
    using ConstIterator = typename decltype(m_vector)::ConstIterator;
    using InsertResult = std::pair<Iterator, bool>; // iterator, was inserted

    FlatMap();
    FlatMap(std::initializer_list<KeyValuePair> initializer_list)
        : m_vector(initializer_list)
    {
        std::sort(
            m_vector.begin(),
            m_vector.end(),
            [](const auto &lhs, const auto &rhs) {
                return lhs.first < rhs.first;
            }
        );
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
    InsertResult Insert(Pair<Key, Value> &&pair);

    InsertResult Set(const Key &key, const Value &value);
    InsertResult Set(const Key &key, Value &&value);
    InsertResult Set(Iterator iter, const Value &value);
    InsertResult Set(Iterator iter, Value &&value);

    template <class ...Args>
    InsertResult Emplace(const Key &key, Args &&... args)
        { return Insert(key, Value(std::forward<Args>(args)...)); }

    bool Erase(Iterator it);
    bool Erase(const Key &key);

    [[nodiscard]] SizeType Size() const { return m_vector.Size(); }
    [[nodiscard]] KeyValuePair *Data() { return m_vector.Data(); }
    [[nodiscard]] KeyValuePair * const Data() const { return m_vector.Data(); }
    [[nodiscard]] bool Any() const { return m_vector.Any(); }
    [[nodiscard]] bool Empty() const { return m_vector.Empty(); }

    void Clear()                                        { m_vector.Clear(); }
    
    [[nodiscard]] KeyValuePair &Front()                 { return m_vector.Front(); }
    [[nodiscard]] const KeyValuePair &Front() const     { return m_vector.Front(); }
    [[nodiscard]] KeyValuePair &Back()                  { return m_vector.Back(); }
    [[nodiscard]] const KeyValuePair &Back() const      { return m_vector.Back(); }

    [[nodiscard]] FlatSet<Key> Keys() const;
    [[nodiscard]] FlatSet<Value> Values() const;

    [[nodiscard]] Value &At(const Key &key)             { return Find(key)->second; }
    [[nodiscard]] const Value &At(const Key &key) const { return Find(key)->second; }

    template <class Lambda>
    [[nodiscard]] bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    template <class Lambda>
    [[nodiscard]] bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    [[nodiscard]] Value &operator[](const Key &key)
    {
        const auto it = Find(key);

        if (it != End()) {
            return it->second;
        }

        return Insert(key, Value { }).first->second;
    }

    HYP_DEF_STL_BEGIN_END(
        m_vector.Begin(),
        m_vector.End()
    )
};

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap() {}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(const FlatMap &other)
    : m_vector(other.m_vector)
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(const FlatMap &other) -> FlatMap &
{
    m_vector = other.m_vector;

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(FlatMap &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(FlatMap &&other) noexcept -> FlatMap&
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::~FlatMap() = default;

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) -> Iterator
{
    const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Contains(const Key &key) const
{
    return Find(key) != End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, const Value &value) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(key);//FlatMap<Key, Value>::Base::LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        auto it = m_vector.Insert(lower_bound, KeyValuePair { key, value });

        return { it, true };
    }

    return { lower_bound, false };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, Value &&value) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(key);//FlatMap<Key, Value>::Base::LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        auto it = m_vector.Insert(lower_bound, KeyValuePair { key, std::move(value) });

        return { it, true };
    }

    return { lower_bound, false };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(Pair<Key, Value> &&pair) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(pair.first);// FlatMap<Key, Value>::Base::LowerBound(pair.first);

    if (lower_bound == End() || !(lower_bound->first == pair.first)) {
        auto it = m_vector.Insert(lower_bound, std::move(pair));

        return { it, true };
    }

    return { lower_bound, false };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key &key, const Value &value) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(key);//FlatMap<Key, Value>::Base::LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        auto it = m_vector.Insert(lower_bound, KeyValuePair { key, value });

        return { it, true };
    }

    lower_bound->second = value;

    return InsertResult{ lower_bound, true };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key &key, Value &&value) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(key);//FlatMap<Key, Value>::Base::LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        auto it = m_vector.Insert(lower_bound, KeyValuePair { key, std::move(value) });

        return { it, true };
    }

    lower_bound->second = std::forward<Value>(value);

    return InsertResult { lower_bound, true};
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(Iterator iter, const Value &value) -> InsertResult
{
    if (iter != End()) {
        iter->second = value;

        return InsertResult { iter, false };
    }

    return InsertResult { iter, true };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(Iterator iter, Value &&value) -> InsertResult
{
    if (iter != End()) {
        iter->second = std::forward<Value>(value);

        return InsertResult { iter, false };
    }

    return InsertResult { iter, true };
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Erase(Iterator it)
{
    if (it == End()) {
        return false;
    }

    m_vector.Erase(it);

    return true;
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Erase(const Key &value)
{
    return Erase(Find(value));
}

template <class Key, class Value>
FlatSet<Key> FlatMap<Key, Value>::Keys() const
{
    FlatSet<Key> keys;

    for (const auto &it : *this) {
        keys.Insert(it.first);    
    }

    return keys;
}

template <class Key, class Value>
FlatSet<Value> FlatMap<Key, Value>::Values() const
{
    FlatSet<Value> values;

    for (const auto &it : *this) {
        values.Insert(it.second);    
    }

    return values;
}

} // namespace hyperion

#endif
