/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HASH_MAP_HPP
#define HYPERION_HASH_MAP_HPP

#include <core/containers/Array.hpp>
#include <core/containers/ContainerBase.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace containers {
namespace detail {

template <class KeyType, class ValueType>
struct HashElement
{
    HashElement                 *next = nullptr;
    HashCode::ValueType         hash_code;

    Pair<KeyType, ValueType>    pair;

    /*! \brief Implement GetHashCode() so GetHashCode() of the entire HashMap can be used. */
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return pair.GetHashCode(); }
};

template <class KeyType, class ValueType>
struct HashBucket
{
    struct ConstIterator;

    struct Iterator
    {
        HashBucket<KeyType, ValueType>  *bucket;
        HashElement<KeyType, ValueType> *element;

        HYP_FORCE_INLINE
        Pair<KeyType, ValueType> *operator->() const
            { return &element->pair; }

        HYP_FORCE_INLINE
        Pair<KeyType, ValueType> &operator*()
            { return element->pair; }

        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> &operator*() const
            { return element->pair; }

        HYP_FORCE_INLINE
        Iterator &operator++()
            { element = element->next; return *this; }

        HYP_FORCE_INLINE
        Iterator operator++(int) const
            { return Iterator { bucket, element->next }; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return element == other.element; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return element != other.element; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return element == other.element; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return element != other.element; }

        HYP_FORCE_INLINE
        operator ConstIterator() const
            { return { bucket, element }; }
    };

    struct ConstIterator
    {
        const HashBucket<KeyType, ValueType>    *bucket;
        const HashElement<KeyType, ValueType>   *element;

        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> *operator->() const
            { return &element->pair; }

        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> &operator*() const
            { return element->pair; }

        HYP_FORCE_INLINE
        ConstIterator &operator++()
            { element = element->next; return *this; }

        HYP_FORCE_INLINE
        ConstIterator operator++(int) const
            { return ConstIterator { bucket, element->next }; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return element == other.element; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return element != other.element; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return element == other.element; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return element != other.element; }
    };
    
    HashElement<KeyType, ValueType> *head = nullptr;
    HashElement<KeyType, ValueType> *tail = nullptr;
    
    Iterator Push(HashElement<KeyType, ValueType> *element)
    {
        if (head == nullptr) {
            head = element;
            tail = head;
        } else {
            tail->next = element;
            tail = tail->next;
        }

        tail->next = nullptr;

        return { this, tail };
    }

    Iterator Find(HashCode::ValueType hash)
    {
        for (auto it = head; it != nullptr; it = it->next) {
            if (it->hash_code == hash) {
                return Iterator { this, it };
            }
        }

        return End();
    }

    ConstIterator Find(HashCode::ValueType hash) const
    {
       for (auto it = head; it != nullptr; it = it->next) {
            if (it->hash_code == hash) {
                return ConstIterator { this, it };
            }
        }

        return End();
    }

    Iterator Begin() { return { this, head }; }
    Iterator End() { return { this, nullptr }; }
    ConstIterator Begin() const { return { this, head }; }
    ConstIterator End() const { return { this, nullptr }; }

    Iterator begin() { return Begin(); }
    Iterator end() { return End(); }
    ConstIterator begin() const { return Begin(); }
    ConstIterator end() const { return End(); }
    ConstIterator cbegin() const { return Begin(); }
    ConstIterator cend() const { return End(); }
};

} // namespace detail

template <class KeyType, class ValueType>
class HashMap : public ContainerBase<HashMap<KeyType, ValueType>, KeyType>
{
    static constexpr bool is_contiguous = false;

    static constexpr SizeType initial_bucket_size = 16;
    static constexpr double desired_load_factor = 0.75;

    template <class IteratorType>
    static inline void AdvanceIteratorBucket(IteratorType &iter)
    {
        // while (iter.bucket_iter.bucket != iter.hm->m_buckets.End() && iter.bucket_iter.element == nullptr) {
        //     ++iter.bucket_iter.bucket;

        //     if (iter.bucket_iter.bucket == iter.hm->m_buckets.End()) {
        //         iter.bucket_iter.element = nullptr;

        //         break;
        //     }

        //     iter.bucket_iter.element = iter.bucket_iter.bucket->head;
        // }

        const auto *end = iter.hm->m_buckets.End();

        while (iter.bucket_iter.element == nullptr && iter.bucket_iter.bucket != end) {
            if (++iter.bucket_iter.bucket == end) {
                break;
            }

            iter.bucket_iter.element = iter.bucket_iter.bucket->head;
        }
    }

    template <class IteratorType>
    static inline void AdvanceIterator(IteratorType &iter)
    {
        iter.bucket_iter.element = iter.bucket_iter.element->next;

        AdvanceIteratorBucket(iter);
    }

public:
    using Base = ContainerBase<HashMap<KeyType, ValueType>, KeyType>;
    using KeyValuePairType = KeyValuePair<KeyType, ValueType>;

    struct ConstIterator;

    struct Iterator
    {
        HashMap                                                     *hm;
        typename detail::HashBucket<KeyType, ValueType>::Iterator   bucket_iter;

        Iterator(HashMap *hm, typename detail::HashBucket<KeyType, ValueType>::Iterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        Iterator(const Iterator &other)                 = default;
        Iterator &operator=(const Iterator &other)      = default;
        Iterator(Iterator &&other) noexcept             = default;
        Iterator &operator=(Iterator &other) &noexcept  = default;
        ~Iterator()                                     = default;
        
        HYP_FORCE_INLINE
        Pair<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE
        Pair<KeyType, ValueType> &operator*()
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        Iterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE
        Iterator operator++(int) const
        {
            Iterator iter(*this);
            AdvanceIterator(iter);

            return iter;
        }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]] HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }

        operator ConstIterator() const
            { return ConstIterator { const_cast<const HashMap *>(hm), typename detail::HashBucket<KeyType, ValueType>::ConstIterator(bucket_iter) }; }
    };

    struct ConstIterator
    {
        const HashMap                                                   *hm;
        typename detail::HashBucket<KeyType, ValueType>::ConstIterator  bucket_iter;

        ConstIterator(const HashMap *hm, typename detail::HashBucket<KeyType, ValueType>::ConstIterator bucket_iter)
            : hm(hm),
              bucket_iter(bucket_iter)
        {
            AdvanceIteratorBucket(*this);
        }

        ConstIterator(const ConstIterator &other)                   = default;
        ConstIterator &operator=(const ConstIterator &other)        = default;
        ConstIterator(ConstIterator &&other) noexcept               = default;
        ConstIterator &operator=(ConstIterator &other) &noexcept    = default;
        ~ConstIterator()                                            = default;
        
        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> *operator->() const
            {  return bucket_iter.operator->(); }
        
        HYP_FORCE_INLINE
        const Pair<KeyType, ValueType> &operator*() const
            {  return bucket_iter.operator*(); }
        
        HYP_FORCE_INLINE
        ConstIterator &operator++()
        {
            AdvanceIterator(*this);

            return *this;
        }
        
        HYP_FORCE_INLINE
        ConstIterator operator++(int) const
        {
            ConstIterator iter = *this;
            AdvanceIterator(iter);

            return iter;
        }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const Iterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const Iterator &other) const
            { return bucket_iter != other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator==(const ConstIterator &other) const
            { return bucket_iter == other.bucket_iter; }

        [[nodiscard]]
        HYP_FORCE_INLINE
        bool operator!=(const ConstIterator &other) const
            { return bucket_iter != other.bucket_iter; }
    };

    using InsertResult = Pair<Iterator, bool>;

    HashMap();

    HashMap(std::initializer_list<KeyValuePairType> initializer_list)
        : HashMap()
    {
        Array<KeyValuePairType> temp(initializer_list);

        for (auto &&item : temp) {
            Set(std::move(item.first), std::move(item.second));
        }
    }

    HashMap(const HashMap &other);
    HashMap &operator=(const HashMap &other);
    HashMap(HashMap &&other) noexcept;
    HashMap &operator=(HashMap &&other) noexcept;
    ~HashMap();

    ValueType &operator[](const KeyType &key);

#ifndef HYP_DEBUG_MODE
    [[nodiscard]] ValueType &At(const KeyType &key)
        { return Find(key)->second; }

    [[nodiscard]] const ValueType &At(const KeyType &key) const
        { return Find(key)->second; }
#else
    [[nodiscard]] ValueType &At(const KeyType &key)
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->second;
    }

    [[nodiscard]] const ValueType &At(const KeyType &key) const
    {
        const auto it = Find(key);
        AssertThrowMsg(it != End(), "At(): Element not found");

        return it->second;
    }
#endif

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Any() const
        { return m_size != 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Empty() const
        { return m_size == 0; }

    [[nodiscard]] bool operator==(const HashMap &other) const
    {
        if (Size() != other.Size()) {
            return false;
        }

        for (const auto &bucket : m_buckets) {
            for (auto it = bucket.head; it != nullptr; it = it->next) {
                const auto other_it = other.Find(it->pair.first);

                if (other_it == other.End()) {
                    return false;
                }

                if (other_it->second != it->pair.second) {
                    return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(const HashMap &other) const
        { return !(*this == other); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Size() const
        { return m_size; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType BucketCount() const
        { return m_buckets.Size(); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Bucket(const KeyType &key) const
        { return HashCode::GetHashCode(key).Value() % m_buckets.Size(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    double LoadFactor() const
        { return double(Size()) / double(BucketCount()); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr double MaxLoadFactor()
        { return desired_load_factor; }

    Iterator Find(const KeyType &key);
    ConstIterator Find(const KeyType &key) const;

    bool Contains(const KeyType &key) const;
    
    Iterator Erase(Iterator iter);
    bool Erase(const KeyType &key);

    void Set(const KeyType &key, const ValueType &value);
    void Set(const KeyType &key, ValueType &&value);
    void Set(KeyType &&key, const ValueType &value);
    void Set(KeyType &&key, ValueType &&value);

    InsertResult Insert(const KeyType &key, const ValueType &value);
    InsertResult Insert(const KeyType &key, ValueType &&value);
    InsertResult Insert(KeyType &&key, const ValueType &value);
    InsertResult Insert(KeyType &&key, ValueType &&value);
    InsertResult Insert(Pair<KeyType, ValueType> &&pair);

    void Clear();
    
    HYP_FORCE_INLINE
    Iterator Begin()
        { return Iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data(), m_buckets[0].head }); }
    
    HYP_FORCE_INLINE
    Iterator End()
        { return Iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { m_buckets.Data() + m_buckets.Size(), nullptr }); }
    
    HYP_FORCE_INLINE
    ConstIterator Begin() const
        { return ConstIterator(this, typename detail::HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data(), m_buckets[0].head }); }
    
    HYP_FORCE_INLINE
    ConstIterator End() const
        { return ConstIterator(this, typename detail::HashBucket<KeyType, ValueType>::ConstIterator { m_buckets.Data() + m_buckets.Size(), nullptr }); }
    
    HYP_FORCE_INLINE
    Iterator begin()
        { return Begin(); }
    
    HYP_FORCE_INLINE
    Iterator end()
        { return End(); }
    
    HYP_FORCE_INLINE
    ConstIterator begin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE
    ConstIterator end() const
        { return End(); }
    
    HYP_FORCE_INLINE
    ConstIterator cbegin() const
        { return Begin(); }
    
    HYP_FORCE_INLINE
    ConstIterator cend() const
        { return End(); }

private:
    void CheckAndRebuildBuckets();

    [[nodiscard]]
    HYP_FORCE_INLINE
    detail::HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash)
        { return &m_buckets[hash % m_buckets.Size()]; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const detail::HashBucket<KeyType, ValueType> *GetBucketForHash(HashCode::ValueType hash) const
        { return &m_buckets[hash % m_buckets.Size()]; }

    void Set_Internal(Pair<KeyType, ValueType> &&pair);
    InsertResult Insert_Internal(Pair<KeyType, ValueType> &&pair);

    Array<detail::HashBucket<KeyType, ValueType>, initial_bucket_size * sizeof(detail::HashBucket<KeyType, ValueType>)> m_buckets;
    SizeType m_size;
};

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap()
    : m_size(0)
{
    m_buckets.Resize(initial_bucket_size);
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(const HashMap &other)
    : m_size(other.m_size)
{
    m_buckets.Resize(other.m_buckets.Size());

    for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++) {
        const auto &bucket = other.m_buckets[bucket_index];

        for (auto it = bucket.head; it != nullptr; it = it->next) {
            m_buckets[bucket_index].Push(new detail::HashElement<KeyType, ValueType> {
                nullptr,
                it->hash_code,
                it->pair
            });
        }
    }
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(const HashMap &other) -> HashMap &
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }
    
    m_size = other.m_size;

    m_buckets.Clear();
    m_buckets.Resize(other.m_buckets.Size());

    for (SizeType bucket_index = 0; bucket_index < other.m_buckets.Size(); bucket_index++) {
        const auto &bucket = other.m_buckets[bucket_index];

        for (auto it = bucket.head; it != nullptr; it = it->next) {
            m_buckets[bucket_index].Push(new detail::HashElement<KeyType, ValueType> {
                nullptr,
                it->hash_code,
                it->pair
            });
        }
    }

    return *this;
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::HashMap(HashMap &&other) noexcept
    : m_buckets(std::move(other.m_buckets)),
      m_size(other.m_size)
{
    other.m_buckets.Resize(initial_bucket_size);
    other.m_size = 0;
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator=(HashMap &&other) noexcept -> HashMap &
{
    m_buckets = std::move(other.m_buckets);
    other.m_buckets.Resize(initial_bucket_size);

    m_size = other.m_size;
    other.m_size = 0;

    return *this;
}

template <class KeyType, class ValueType>
HashMap<KeyType, ValueType>::~HashMap()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::operator[](const KeyType &key) -> ValueType &
{
    const Iterator it = Find(key);

    if (it != End()) {
        return it->second;
    }

    return Insert(key, ValueType { }).first->second;
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::CheckAndRebuildBuckets()
{
    // Check load factor, if currently load factor is greater than `load_factor`, then rehash so that the load factor becomes <= `load_factor` constant.

    if (LoadFactor() < MaxLoadFactor()) {
        return;
    }

    const SizeType new_bucket_count = SizeType(double(BucketCount()) / MaxLoadFactor());

    Array<detail::HashBucket<KeyType, ValueType>, initial_bucket_size * sizeof(detail::HashBucket<KeyType, ValueType>)> new_buckets;
    new_buckets.Resize(new_bucket_count);

    for (auto &bucket : m_buckets) {
        for (auto it = bucket.head; it != nullptr;) {
            detail::HashBucket<KeyType, ValueType> *new_bucket = new_buckets.Data() + (it->hash_code % new_buckets.Size());

            detail::HashElement<KeyType, ValueType> *next = it->next;
            it->next = nullptr;

            new_bucket->Push(it);

            it = next;
        }
    }

    m_buckets = std::move(new_buckets);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) -> Iterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    typename detail::HashBucket<KeyType, ValueType>::Iterator it = bucket->Find(hash_code);

    if (it == bucket->End()) {
        return End();
    }

    return Iterator(this, it);
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Find(const KeyType &key) const -> ConstIterator
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(key).Value();
    const detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    const typename detail::HashBucket<KeyType, ValueType>::ConstIterator it = bucket->Find(hash_code);

    if (it == bucket->End()) {
        return End();
    }

    return ConstIterator(this, it);
}

template <class KeyType, class ValueType>
bool HashMap<KeyType, ValueType>::Contains(const KeyType &key) const
{
    return Find(key) != End();
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Erase(Iterator iter) -> Iterator
{
    if (iter == End()) {
        return End();
    }

    --m_size;

    detail::HashElement<KeyType, ValueType> *prev = nullptr;

    for (auto it = iter.bucket_iter.bucket->head; it != nullptr && it != iter.bucket_iter.element; it = it->next) {
        prev = it;
    }

    if (iter.bucket_iter.element == iter.bucket_iter.bucket->head) {
        iter.bucket_iter.bucket->head = iter.bucket_iter.element->next;
    }

    if (iter.bucket_iter.element == iter.bucket_iter.bucket->tail) {
        iter.bucket_iter.bucket->tail = prev;
    }

    if (prev != nullptr) {
        prev->next = iter.bucket_iter.element->next;
    }

    Iterator next_iterator(this, typename detail::HashBucket<KeyType, ValueType>::Iterator { iter.bucket_iter.bucket, iter.bucket_iter.element->next });

    delete iter.bucket_iter.element;

    return next_iterator;
}

template <class KeyType, class ValueType>
bool HashMap<KeyType, ValueType>::Erase(const KeyType &key)
{
    const Iterator it = Find(key);

    if (it == End()) {
        return false;
    }
    
    Erase(it);

    return true;
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set_Internal(Pair<KeyType, ValueType> &&pair)
{
    const HashCode::ValueType hash_code = HashCode::GetHashCode(pair.first).Value();

    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(hash_code);

    if (it != bucket->End()) {
        *it = std::move(pair);
    } else {
        bucket->Push(new detail::HashElement<KeyType, ValueType> {
            nullptr,
            hash_code,
            std::move(pair)
        });

        m_size++;

        CheckAndRebuildBuckets();
    }
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert_Internal(Pair<KeyType, ValueType> &&pair) -> InsertResult
{
    // Have to rehash before any insertion, so we don't invalidate the iterator or bucket pointer.
    CheckAndRebuildBuckets();

    const HashCode::ValueType hash_code = HashCode::GetHashCode(pair.first).Value();

    detail::HashBucket<KeyType, ValueType> *bucket = GetBucketForHash(hash_code);

    auto it = bucket->Find(hash_code);

    Iterator insert_it(this, it);

    if (it != bucket->End()) {
        return InsertResult { insert_it, false };
    }

    insert_it.bucket_iter = bucket->Push(new detail::HashElement<KeyType, ValueType> {
        nullptr,
        hash_code,
        std::move(pair)
    });
    
    m_size++;

    return InsertResult { insert_it, true };
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, const ValueType &value)
{
    Set_Internal(Pair<KeyType, ValueType> {
        key,
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(const KeyType &key, ValueType &&value)
{
    Set_Internal(Pair<KeyType, ValueType> {
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, const ValueType &value)
{
    Set_Internal(Pair<KeyType, ValueType> {
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Set(KeyType &&key, ValueType &&value)
{
    Set_Internal(Pair<KeyType, ValueType> {
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, const ValueType &value) -> InsertResult
{
    return Insert_Internal(Pair<KeyType, ValueType> {
        key,
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(const KeyType &key, ValueType &&value) -> InsertResult
{
    return Insert_Internal(Pair<KeyType, ValueType> {
        key,
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, const ValueType &value) -> InsertResult
{
    return Insert_Internal(Pair<KeyType, ValueType> {
        std::move(key),
        value
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(KeyType &&key, ValueType &&value) -> InsertResult
{
    return Insert_Internal(Pair<KeyType, ValueType> {
        std::move(key),
        std::move(value)
    });
}

template <class KeyType, class ValueType>
auto HashMap<KeyType, ValueType>::Insert(Pair<KeyType, ValueType> &&pair) -> InsertResult
{
    return Insert_Internal(std::move(pair));
}

template <class KeyType, class ValueType>
void HashMap<KeyType, ValueType>::Clear()
{
    for (auto buckets_it = m_buckets.Begin(); buckets_it != m_buckets.End(); ++buckets_it) {
        for (auto element_it = buckets_it->head; element_it != nullptr;) {
            auto *head = element_it;
            auto *next = head->next;

            delete head;

            element_it = next;
        }
    }

    m_buckets.Clear();
    m_buckets.Resize(initial_bucket_size);

    m_size = 0;
}

} // namespace containers

using containers::HashMap;

} // namespace hyperion

#endif
