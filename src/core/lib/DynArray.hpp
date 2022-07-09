#ifndef HYPERION_V2_LIB_DYN_ARRAY_H
#define HYPERION_V2_LIB_DYN_ARRAY_H

#include "ContainerBase.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {

template <class T>
class DynArray : public ContainerBase<DynArray<T>, UInt> {
    using SizeType  = UInt64;
    using ValueType = T;

public:
    using Iterator      = T *;
    using ConstIterator = const T *;

    DynArray();
    DynArray(SizeType size);
    DynArray(const DynArray &other);
    DynArray(DynArray &&other) noexcept;
    ~DynArray();

    DynArray &operator=(const DynArray &other);
    DynArray &operator=(DynArray &&other) noexcept;

    [[nodiscard]] SizeType Size() const                                       { return m_size; }

    [[nodiscard]] ValueType *Data()                                           { return reinterpret_cast<T *>(m_buffer); }
    [[nodiscard]] const ValueType *Data() const                               { return reinterpret_cast<const T *>(m_buffer); }

    [[nodiscard]] ValueType &Front()                                          { return m_buffer[0].Get(); }
    [[nodiscard]] const ValueType &Front() const                              { return m_buffer[0].Get(); }

    [[nodiscard]] ValueType &Back()                                           { return m_buffer[m_size - 1].Get(); }
    [[nodiscard]] const ValueType &Back() const                               { return m_buffer[m_size - 1].Get(); }

    ValueType &operator[](typename DynArray::Base::KeyType index)             { return m_buffer[index].Get(); }
    const ValueType &operator[](typename DynArray::Base::KeyType index) const { return m_buffer[index].Get(); }

    [[nodiscard]] bool Empty() const                                          { return m_size == 0; }
    [[nodiscard]] bool Any() const                                            { return m_size != 0; }

    void Reserve(SizeType capacity);
    void Resize(SizeType new_size);
    // fit capacity to size
    void Refit();
    void PushBack(const ValueType &value);
    void PushBack(ValueType &&value);
    void PushBack(SizeType n, ValueType *values);
    Iterator Erase(ConstIterator iter);
    Iterator EraseAt(typename DynArray::Base::KeyType index);
    void Pop();
    void Clear();
    
    template <class ...Args>
    void EmplaceBack(Args &&... args)
    {
        const SizeType index = m_size;

        if (m_size + 1 >= m_capacity) {
            Reserve(1ull << static_cast<SizeType>(std::ceil(std::log(m_size + 1) / std::log(2.0))));
        }

        // set item at index
        new (&m_buffer[index].data_buffer) T(std::forward<Args>(args)...);
        m_size++;
    }

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<ValueType *>(&m_buffer[0]),
        reinterpret_cast<ValueType *>(&m_buffer[m_size])
    )

private:
    void SetCapacity(SizeType capacity);

    SizeType     m_size;
    SizeType     m_capacity;
    
    struct Storage {
        using StorageType = std::aligned_storage_t<sizeof(T), alignof(T)>;

        StorageType data_buffer;

        ValueType &Get()
        {
            return *reinterpret_cast<T *>(&data_buffer);
        }

        [[nodiscard]] const ValueType &Get() const
        {
            return *reinterpret_cast<const T *>(&data_buffer);
        }
    };

    union {
        Storage   *m_buffer;
        ValueType *m_values;
    };
};

template <class T>
DynArray<T>::DynArray()
    : m_size(0),
      m_capacity(1ull << static_cast<SizeType>(std::ceil(std::log(0) / std::log(2.0)))),
      m_buffer(new Storage[m_capacity])
{
}

template <class T>
DynArray<T>::DynArray(SizeType size)
    : m_size(size),
      m_capacity(1ull << static_cast<SizeType>(std::ceil(std::log(size) / std::log(2.0)))),
      m_buffer(new Storage[m_capacity])
{
    for (SizeType i = 0; i < m_size; i++) {
        new (&m_buffer[i].data_buffer) T();
    }
}

template <class T>
DynArray<T>::DynArray(const DynArray &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(new Storage[other.m_capacity])
{
    // copy all members
    for (SizeType i = 0; i < m_size; i++) {
        new (&m_buffer[i].data_buffer) T(other.m_buffer[i].Get());
    }
}

template <class T>
DynArray<T>::DynArray(DynArray &&other) noexcept
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(other.m_buffer)
{
    other.m_size     = 0;
    other.m_capacity = 0;
    other.m_buffer   = nullptr;
}


template <class T>
DynArray<T>::~DynArray()
{
    for (Int64 i = m_size - 1; i >= 0; --i) {
        m_buffer[i].Get().~T();
    }
    
    // only nullptr if it has been move()'d in which case size would be 0
    delete[] m_buffer;
}

template <class T>
auto DynArray<T>::operator=(const DynArray &other) -> DynArray&
{
    if (this == std::addressof(other)) {
        return *this;
    }
    
    for (Int64 i = m_size - 1; i >= 0; --i) {
        m_buffer[i].Get().~T();
    }

    delete[] m_buffer;

    m_size     = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer   = new Storage[other.m_capacity];

    // copy all objects
    for (SizeType i = 0; i < m_size; i++) {
        new (&m_buffer[i].data_buffer) T(other.m_buffer[i].Get());
    }

    return *this;
}

template <class T>
auto DynArray<T>::operator=(DynArray &&other) noexcept -> DynArray&
{
    for (Int64 i = m_size - 1; i >= 0; --i) {
        m_buffer[i].Get().~T();
    }

    delete[] m_buffer;

    m_size     = other.m_size;
    m_capacity = other.m_capacity;
    m_buffer   = other.m_buffer;

    other.m_size     = 0;
    other.m_capacity = 0;
    other.m_buffer   = nullptr;

    return *this;
}

template <class T>
void DynArray<T>::SetCapacity(SizeType capacity)
{
    // delete and copy all over again
    m_capacity       = capacity;
    auto *new_buffer = new Storage[m_capacity];

    AssertThrow(m_size <= m_capacity);
    
    for (SizeType i = 0; i < m_size; i++) {
        if constexpr (std::is_move_assignable_v<T>) {
            new_buffer[i].Get() = std::move(m_buffer[i].Get());
        } else {
            new_buffer[i].Get() = m_buffer[i].Get();
        }
    }

    // delete old buffer
    for (Int64 i = m_size - 1; i >= 0; --i) {
        m_buffer[i].Get().~T();
    }

    delete[] m_buffer;

    // set internal buffer to the new one
    m_buffer = new_buffer;
}

template <class T>
void DynArray<T>::Reserve(SizeType capacity)
{
    if (m_capacity >= capacity) {
        return;
    }

    SetCapacity(capacity);
}

template <class T>
void DynArray<T>::Refit()
{
    if (m_capacity == m_size) {
        return;
    }

    SetCapacity(m_size);
}

template <class T>
void DynArray<T>::Resize(SizeType new_size)
{
    if (new_size == m_size) {
        return;
    }

    if (new_size > m_size) {
        Reserve(1ull << static_cast<SizeType>(std::ceil(std::log(new_size) / std::log(2.0))));

        for (; m_size < new_size; ++m_size) {
            new (&m_buffer[m_size].data_buffer) T();
        }

        return;
    }

    for (; m_size > new_size; --m_size) {
        m_buffer[m_size - 1].Get().~T();
    }
}

template <class T>
void DynArray<T>::PushBack(const ValueType &value)
{
    const SizeType index = m_size;

    if (m_size + 1 >= m_capacity) {
        Reserve(1ull << static_cast<SizeType>(std::ceil(std::log(m_size + 1) / std::log(2.0))));
    }

    // set item at index
    new (&m_buffer[index].data_buffer) T(value);
    m_size++;
}

template <class T>
void DynArray<T>::PushBack(ValueType &&value)
{
    const SizeType index = m_size;

    if (m_size + 1 >= m_capacity) {
        Reserve(1ull << static_cast<SizeType>(std::ceil(std::log(m_size + 1) / std::log(2.0))));
    }

    // set item at index
    new (&m_buffer[index].data_buffer) T(std::forward<ValueType>(value));
    m_size++;
}

template <class T>
void DynArray<T>::PushBack(SizeType n, ValueType *values)
{
    const SizeType index = m_size;

    if (m_size + n >= m_capacity) {
        // delete and copy all over again
        Reserve(1ull << static_cast<SizeType>(std::ceil(std::log(m_size + n) / std::log(2.0))));
    }

    for (SizeType i = 0; i < n; i++) {
        // set item at index
        new (&m_buffer[index + i].data_buffer) T(values[i]);
    }

    m_size += n;
}

template <class T>
auto DynArray<T>::Erase(ConstIterator iter) -> Iterator
{
    if (iter == End()) {
        return End();
    }

    const Int64 dist = iter - Begin();

    for (Int64 index = dist; index < Size() - 1; ++index) {
        if constexpr (std::is_move_assignable_v<T>) {
            m_buffer[index].Get() = std::move(m_buffer[index + 1].Get());
        } else {
            m_buffer[index].Get() = m_buffer[index + 1].Get();
        }
    }

    Pop();

    return Begin() + dist;
}

template <class T>
auto DynArray<T>::EraseAt(typename DynArray::Base::KeyType index) -> Iterator
{
    return Erase(Begin() + index);
}

template <class T>
void DynArray<T>::Pop()
{
    AssertThrow(m_size != 0);
    // manual destructor call
    m_buffer[m_size - 1].Get().~T();
    m_size--;
}

template <class T>
void DynArray<T>::Clear()
{
    while (m_size) {
        // manual destructor call
        m_buffer[m_size - 1].Get().~T();
        --m_size;
    }
}

} // namespace hyperion

#endif