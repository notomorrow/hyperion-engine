/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARRAY_HPP
#define HYPERION_ARRAY_HPP

#include <core/containers/ContainerBase.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/utilities/Pair.hpp>
#include <core/Defines.hpp>
#include <core/memory/Memory.hpp>
#include <core/utilities/ValueStorage.hpp>

#include <core/system/Debug.hpp>

#include <Types.hpp>
#include <HashCode.hpp>
#include <math/MathUtil.hpp>

#include <algorithm>
#include <utility>
#include <cmath>

namespace hyperion {

namespace containers {

template <class T, SizeType NumInlineBytes = 256u>
class Array;

/*! \brief Array class with smart front removal and inline storage so small lists
    do not require any heap allocation
    \details Average speed is about the same as std::vector in most cases
    \note will use a bit more memory than std::vector, partially because of inline storage in the class,
    and partially due to the front offset member, used to have fewer deallocations/shifting on PopFront(). */
template <class T, SizeType NumInlineBytes>
class Array : public ContainerBase<Array<T, NumInlineBytes>, SizeType>
{
public:
    using Base = ContainerBase<Array<T, NumInlineBytes>, SizeType>;
    using KeyType = typename Base::KeyType;
    using ValueType = T;
    using Storage = ValueStorage<ValueType>;

    static constexpr bool is_contiguous = true;
    
    static constexpr bool is_pod_type = IsPODType<T>;
    
    static constexpr bool use_inline_storage = sizeof(T) <= NumInlineBytes && NumInlineBytes != 0;
    static constexpr SizeType num_inline_bytes = use_inline_storage ? NumInlineBytes : 0;
    static constexpr SizeType num_inline_elements = use_inline_storage ? (num_inline_bytes / sizeof(T)) : 0;

protected:
    // on PushFront() we can pad the start with this number,
    // so when multiple successive calls to PushFront() happen,
    // we're not realloc'ing everything each time
    static constexpr SizeType push_front_padding = 4;

public:
    using Iterator = T *;
    using ConstIterator = const T *;
    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    Array();

    template <SizeType Sz>
    Array(T const (&items)[Sz])
        : Array()
    {
        ResizeUninitialized(Sz);
        
        auto *storage_ptr = Data();

        for (SizeType i = 0; i < Sz; ++i) {
            Memory::Construct<T>(storage_ptr++, items[i]);
        }
    }

    template <SizeType Sz>
    Array(T (&&items)[Sz])
        : Array()
    {
        ResizeUninitialized(Sz);
        
        auto *storage_ptr = Data();

        for (SizeType i = 0; i < Sz; ++i) {
            Memory::Construct<T>(storage_ptr++, std::move(items[i]));
        }
    }

    template <SizeType Sz>
    Array(const FixedArray<T, Sz> &items)
        : Array(items.Begin(), items.End())
    {
    }

    template <SizeType Sz>
    Array(FixedArray<T, Sz> &&items)
        : Array()
    {
        ResizeUninitialized(Sz);

        auto *storage_ptr = Data();

        for (SizeType i = 0; i < Sz; ++i) {
            Memory::Construct<T>(storage_ptr++, std::move(items[i]));
        }
    }

    Array(T *ptr, SizeType size)
        : Array()
    {
        ResizeUninitialized(size);
        
        auto *storage_ptr = Data();

        const T *first = ptr;
        const T *last = ptr + size;

        for (auto it = first; it != last; ++it) {
            Memory::Construct<T>(storage_ptr++, *it);
        }
    }

    Array(Iterator first, Iterator last)
        : Array()
    {
        const SizeType dist = last - first;
        ResizeUninitialized(dist);
        
        auto *storage_ptr = Data();

        for (auto it = first; it != last; ++it) {
            Memory::Construct<T>(storage_ptr++, *it);
        }
    }

    Array(ConstIterator first, ConstIterator last)
        : Array()
    {
        const SizeType dist = last - first;
        ResizeUninitialized(dist);
        
        auto *storage_ptr = Data();

        for (auto it = first; it != last; ++it) {
            Memory::Construct<T>(storage_ptr++, *it);
        }
    }

    Array(const T *ptr, SizeType size)
        : Array(ptr, ptr + size)
    {
    }
    
    Array(std::initializer_list<T> initializer_list)
        : Array(initializer_list.begin(), initializer_list.end())
    {
    }

    Array(const Array &other);
    Array(Array &&other) noexcept;
    ~Array();

    Array &operator=(const Array &other);
    Array &operator=(Array &&other) noexcept;

    /*! \brief Returns the number of elements in the array. */
    [[nodiscard]] SizeType Size() const { return m_size - m_start_offset; }

    /*! \brief Returns the size in bytes of the array. */
    [[nodiscard]] SizeType ByteSize() const { return (m_size - m_start_offset) * sizeof(T); }

    /*! \brief Returns a pointer to the first element in the array. */
    [[nodiscard]] ValueType *Data() { return &GetBuffer()[m_start_offset]; }

    /*! \brief Returns a pointer to the first element in the array. */
    [[nodiscard]] const ValueType *Data() const { return &GetBuffer()[m_start_offset]; }

    /*! \brief Returns a reference to the first element in the array. */
    [[nodiscard]] ValueType &Front() { return GetBuffer()[m_start_offset]; }
    
    /*! \brief Returns a reference to the first element in the array. */
    [[nodiscard]] const ValueType &Front() const { return GetBuffer()[m_start_offset]; }

    /*! \brief Returns a reference to the last element in the array.  */
    [[nodiscard]] ValueType &Back() { return GetBuffer()[m_size - 1]; }

    /*! \brief Returns a reference to the last element in the array. */
    [[nodiscard]] const ValueType &Back() const { return GetBuffer()[m_size - 1]; }

    /*! \brief Returns true if the array has no elements. */
    [[nodiscard]] bool Empty() const { return Size() == 0; }

    /*! \brief Returns true if the array has any elements. */
    [[nodiscard]] bool Any() const { return Size() != 0; }

    /*! \brief Returns the element at the given index. No bounds checking is performed. */
    ValueType &operator[](KeyType index) { return GetBuffer()[m_start_offset + index]; }

    /*! \brief Returns the element at the given index. No bounds checking is performed. */
    [[nodiscard]] const ValueType &operator[](KeyType index) const { return GetBuffer()[m_start_offset + index]; }

    /*! \brief Reserves enough space for {capacity} elements. If the capacity is smaller than the current capacity, nothing happens. */
    void Reserve(SizeType capacity);

    /*! \brief Resizes the array to the given size. If the size is smaller than the current size, the array is truncated. */
    void Resize(SizeType new_size);

    /*! \brief Refits the array to the smallest possible size. This is useful if you have a large array and want to free up memory. */
    void Refit();

    /*! \brief Updates the capacity of the array to be at least {capacity} */
    void SetCapacity(SizeType capacity, SizeType copy_offset = 0);

    ValueType &PushBack(const ValueType &value);
    ValueType &PushBack(ValueType &&value);

    /*! \brief Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time.
        */
    ValueType &PushFront(const ValueType &value);

    /*! \brief Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time. */
    ValueType &PushFront(ValueType &&value);

    /*! \brief Shift the array to the left by {count} times */
    void Shift(SizeType count);

    Array<T, NumInlineBytes> Slice(int first, int last) const;

    void Concat(const Array &other);
    void Concat(Array &&other);
    /*! \brief Erase an element by iterator. */
    Iterator Erase(ConstIterator iter);

    /*! \brief Erase an element by value. A Find() is performed, and if the result is not equal to End(),
     *  the element is removed. */
    Iterator Erase(const T &value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType &value);
    Iterator Insert(ConstIterator where, ValueType &&value);
    ValueType PopFront();
    ValueType PopBack();
    void Clear();

    /*! \brief Returns true if any elements in the array satisfy the given lambda. */
    template <class Lambda>
    [[nodiscard]] bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    /*! \brief Returns true if all elements in the array satisfy the given lambda. */
    template <class Lambda>
    [[nodiscard]] bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    template <SizeType OtherNumInlineBytes>
    [[nodiscard]] bool operator==(const Array<T, OtherNumInlineBytes> &other) const
    {
        if (std::addressof(other) == this) {
            return true;
        }

        if (Size() != other.Size()) {
            return false;
        }

        auto it = Begin();
        auto other_it = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++other_it) {
            if (!(*it == *other_it)) {
                return false;
            }
        }

        return true;
    }

    template <SizeType OtherNumInlineBytes>
    [[nodiscard]] bool operator!=(const Array<T, OtherNumInlineBytes> &other) const
        { return !operator==(other); }
    
    HYP_DEF_STL_BEGIN_END(
        GetBuffer() + m_start_offset,
        GetBuffer() + m_size
    )

protected:
    HYP_FORCE_INLINE
    T *GetBuffer()
    {
        if constexpr (use_inline_storage) {
            return reinterpret_cast<T *>(m_is_dynamic ? m_buffer : &m_inline_storage[0]);
        } else {
            return reinterpret_cast<T *>(m_buffer);
        }
    }

    HYP_FORCE_INLINE
    const T *GetBuffer() const
    {
        if constexpr (use_inline_storage) {
            return reinterpret_cast<const T *>(m_is_dynamic ? m_buffer : &m_inline_storage[0]);
        } else {
            return reinterpret_cast<const T *>(m_buffer);
        }
    }

    HYP_FORCE_INLINE
    Storage *GetStorage()
    {
        if constexpr (use_inline_storage) {
            return m_is_dynamic ? m_buffer : &m_inline_storage[0];
        } else {
            return m_buffer;
        }
    }

    HYP_FORCE_INLINE
    const Storage *GetStorage() const
    {
        if constexpr (use_inline_storage) {
            return m_is_dynamic ? m_buffer : &m_inline_storage[0];
        } else {
            return m_buffer;
        }
    }

    void ResizeUninitialized(SizeType new_size);

    void ResetOffsets();

    auto RealBegin() const { return ToValueTypePtr(GetStorage()[0]); }
    auto RealEnd() const { return ToValueTypePtr(GetStorage()[m_size]); }

    static SizeType GetCapacity(SizeType size)
    {
        return 1ull << static_cast<SizeType>(std::ceil(std::log(size) / std::log(2.0)));
    }

    SizeType m_size;
    SizeType m_capacity;

    static_assert(sizeof(Storage) == sizeof(Storage::data_buffer), "Storage struct should not be padded");
    // static_assert(alignof(Storage) == alignof(Storage::data_buffer), "Storage struct should not be padded");

    HYP_FORCE_INLINE
    static ValueType &ToValueType(Storage &storage)
    {
        return *reinterpret_cast<ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static const ValueType &ToValueType(const Storage &storage)
    {
        return *reinterpret_cast<const ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static ValueType *ToValueTypePtr(Storage &storage)
    {
        return reinterpret_cast<ValueType *>(&storage);
    }

    HYP_FORCE_INLINE
    static const ValueType *ToValueTypePtr(const Storage &storage)
    {
        return reinterpret_cast<const ValueType *>(&storage);
    }

private:
    union {
        // dynamic memory
        Storage                                             *m_buffer;
        ValueStorageArray<ValueType, num_inline_elements>   m_inline_storage;

        // for debugging
        ValueType                                           *m_buffer_raw;
    };

protected:
    SizeType                                                m_start_offset;
    bool                                                    m_is_dynamic;
};

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array()
    : m_size(0),
      m_capacity(num_inline_elements),
      m_buffer(nullptr),
      m_is_dynamic(false),
      m_start_offset(0)
{
#ifdef HYP_DEBUG_MODE
    if constexpr (use_inline_storage) {
        Memory::Garble(&m_inline_storage[0], num_inline_elements * sizeof(Storage));
    }
#endif
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array(const Array &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(nullptr),
      m_is_dynamic(other.m_is_dynamic),
      m_start_offset(other.m_start_offset)
{
    if (m_is_dynamic) {
        m_buffer = static_cast<Storage *>(Memory::Allocate(sizeof(Storage) * other.m_capacity));
    }

    auto *buffer = GetStorage();

    // copy all members
    for (SizeType i = m_start_offset; i < m_size; ++i) {
        Memory::Construct<T>(&buffer[i].data_buffer, other.GetStorage()[i].Get());
    }
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array(Array &&other) noexcept
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_buffer(nullptr),
      m_is_dynamic(other.m_is_dynamic),
      m_start_offset(other.m_start_offset)
{
    if (m_is_dynamic) {
        m_buffer = other.m_buffer;
    } else {
        auto *buffer = GetStorage();

        // move all members
        for (SizeType i = m_start_offset; i < m_size; ++i) {
            Memory::Construct<T>(&buffer[i].data_buffer, std::move(other.GetStorage()[i].Get()));
        }
    }

    other.m_size = 0;
    other.m_capacity = num_inline_elements;
    other.m_buffer = nullptr;
    other.m_is_dynamic = false;
    other.m_start_offset = 0;

#ifdef HYP_DEBUG_MODE
    if constexpr (use_inline_storage) {
        Memory::Garble(&other.m_inline_storage[0], num_inline_elements * sizeof(Storage));
    }
#endif
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::~Array()
{
    auto *buffer = GetStorage();

    for (SizeType i = m_size; i > m_start_offset;) {
        Memory::Destruct(buffer[--i].Get());
    }
    
    // only nullptr if it has been move()'d in which case size would be 0
    //delete[] m_buffer;
    if (m_is_dynamic) {
        Memory::Free(m_buffer);
        m_buffer = nullptr;
    }
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::operator=(const Array &other) -> Array&
{
    if (this == std::addressof(other)) {
        return *this;
    }

    auto *buffer = GetStorage();
    auto *other_buffer = other.GetStorage();

    if (m_capacity < other.m_capacity) {
        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(buffer[--i].Get());
        }

        if (m_is_dynamic) {
            Memory::Free(m_buffer);
            m_buffer = nullptr;
        }

        if (other.m_is_dynamic) {
            m_is_dynamic = true;
            m_buffer = static_cast<Storage *>(Memory::Allocate(sizeof(Storage) * other.m_capacity));
            m_capacity = other.m_capacity;
        } else {
            m_buffer = nullptr;
            m_is_dynamic = false;
            m_capacity = num_inline_elements;
        }

        m_size = other.m_size;
        m_start_offset = other.m_start_offset;

        buffer = GetStorage();

        // copy all objects
        for (SizeType i = m_start_offset; i < m_size; ++i) {
            Memory::Construct<T>(&buffer[i].data_buffer, other_buffer[i].Get());
        }
    } else {
        const SizeType current_size = Size();
        const SizeType other_size = other.Size();

        for (SizeType i = m_start_offset; i < m_size; i++) {
            Memory::Destruct<T>(&buffer[i].data_buffer);
        }

        for (SizeType i = 0; i < other_size; i++) {
            Memory::Construct<T>(&buffer[i].data_buffer, other_buffer[other.m_start_offset + i].Get());
        }

        m_start_offset = 0;
        m_size = other_size;
    }

    return *this;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::operator=(Array &&other) noexcept -> Array&
{
    {
        auto *buffer = GetStorage();

        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(buffer[--i].Get());
        }

        if (m_is_dynamic) {
            Memory::Free(buffer);
            m_buffer = nullptr;
        }
    }
    
    if (other.m_is_dynamic) {
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_buffer = other.m_buffer;
        m_start_offset = other.m_start_offset;
        m_is_dynamic = true;
    } else {
        m_capacity = num_inline_elements;
        m_is_dynamic = false;

        if constexpr (use_inline_storage) {
            // move items individually
            for (SizeType i = 0; i < other.Size(); ++i) {
                Memory::Construct<T>(&m_inline_storage[i].data_buffer, std::move(other.m_inline_storage[other.m_start_offset + i].Get()));
            }
        }

        m_size = other.Size();
        m_start_offset = 0;
        
        if constexpr (use_inline_storage) {
            // manually call destructors
            for (SizeType i = other.m_size; i > other.m_start_offset;) {
                Memory::Destruct(other.m_inline_storage[--i].Get());
            }
        }
    }

    other.m_size = 0;
    other.m_capacity = num_inline_elements;
    other.m_is_dynamic = false;
    other.m_buffer = nullptr;
    other.m_start_offset = 0;

    return *this;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::ResetOffsets()
{
    if (m_start_offset == 0) {
        return;
    }

    auto *buffer = GetStorage();

    // shift all items to left
    for (SizeType index = m_start_offset; index < m_size; ++index) {
        const auto move_index = index - m_start_offset;

        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[move_index].data_buffer, std::move(buffer[index].Get()));
        } else {
            Memory::Construct<T>(&buffer[move_index].data_buffer, buffer[index].Get());
        }

        // manual destructor call
        Memory::Destruct(buffer[index].Get());
    }

    m_size -= m_start_offset;
    m_start_offset = 0;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::SetCapacity(SizeType capacity, SizeType copy_offset)
{
    auto *old_buffer = GetStorage();

    if (capacity > num_inline_elements) {
        // delete and copy all over again
        auto *new_buffer = static_cast<Storage *>(Memory::Allocate(sizeof(Storage) * capacity));

        // AssertThrow(Size() <= m_capacity);
        
        for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
            if constexpr (std::is_move_constructible_v<T>) {
                Memory::Construct<T>(&new_buffer[i].data_buffer, std::move(old_buffer[j].Get()));
            } else {
                Memory::Construct<T>(&new_buffer[i].data_buffer, old_buffer[j].Get());
            }
        }

        // manually call destructors of old buffer
        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(old_buffer[--i].Get());
        }

        if (m_is_dynamic) {
            // delete old buffer memory
            Memory::Free(old_buffer);
        }

        // set internal buffer to the new one
        m_capacity = capacity;
        m_size -= static_cast<int64>(m_start_offset) - static_cast<int64>(copy_offset);
        m_buffer = new_buffer;
        m_is_dynamic = true;
        m_start_offset = copy_offset;
    } else {
        if (m_is_dynamic) { // switch from dynamic to non-dynamic
            if constexpr (use_inline_storage) {
                for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
                    m_inline_storage[i].Get() = std::move(old_buffer[j].Get());
                }
            }

            // call destructors on old buffer
            for (SizeType i = m_size; i > m_start_offset;) {
                Memory::Destruct(old_buffer[--i].Get());
            }

            Memory::Free(old_buffer);

            m_is_dynamic = false;
            m_capacity = num_inline_elements;
        } else if (m_start_offset != copy_offset) {
            if constexpr (use_inline_storage) {
                if (m_start_offset > copy_offset) {
                    const SizeType diff = m_start_offset - copy_offset;

                    // shift left
                    for (SizeType index = m_start_offset; index < m_size; ++index) {
                        const auto move_index = index - diff;

                        if constexpr (std::is_move_constructible_v<T>) {
                            Memory::Construct<T>(&m_inline_storage[move_index].data_buffer, std::move(m_inline_storage[index].Get()));
                        } else {
                            Memory::Construct<T>(&m_inline_storage[move_index].data_buffer, m_inline_storage[index].Get());
                        }

                        // manual destructor call
                        Memory::Destruct(m_inline_storage[index].Get());
                    }
                } else {
                    // shift right
                    const SizeType diff = copy_offset - m_start_offset;

                    for (SizeType index = m_size; index > m_start_offset;) {
                        --index;

                        const auto move_index = index + diff;

                        if constexpr (std::is_move_constructible_v<T>) {
                            Memory::Construct<T>(&m_inline_storage[move_index].data_buffer, std::move(m_inline_storage[index].Get()));
                        } else {
                            Memory::Construct<T>(&m_inline_storage[move_index].data_buffer, m_inline_storage[index].Get());
                        }

                        // manual destructor call
                        Memory::Destruct(m_inline_storage[index].Get());
                    }
                }
            }
        }

        m_size -= static_cast<int64>(m_start_offset) - static_cast<int64>(copy_offset);
        m_start_offset = copy_offset;

        // not currently dynamic; no need to reduce capacity of inline buffer
    }
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Reserve(SizeType capacity)
{
    if (m_capacity >= capacity) {
        return;
    }

    SetCapacity(capacity);
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Resize(SizeType new_size)
{
    const SizeType current_size = Size();

    if (new_size == current_size) {
        return;
    }

    if (new_size > current_size) {
        const SizeType diff = new_size - current_size;

        if (m_size + diff >= m_capacity) {
            if (m_capacity >= current_size + diff) {
                ResetOffsets();
            } else {
                SetCapacity(GetCapacity(current_size + diff));
            }
        }

        auto *buffer = GetStorage();

        while (Size() < new_size) {
            // construct item at index
            Memory::Construct<T>(&buffer[m_size++].data_buffer);
        }
    } else {
        const SizeType diff = current_size - new_size;

        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(GetStorage()[--i].Get());
        }

        m_size -= diff;
    }
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::ResizeUninitialized(SizeType new_size)
{
    const SizeType current_size = Size();

    if (new_size == current_size) {
        return;
    }

    if (new_size > current_size) {
        const SizeType diff = new_size - current_size;

        if (m_size + diff >= m_capacity) {
            if (m_capacity >= current_size + diff) {
                ResetOffsets();
            } else {
                SetCapacity(GetCapacity(current_size + diff));
            }
        }

        m_size += diff;
    } else {
        const SizeType diff = current_size - new_size;

        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(GetStorage()[--i].Get());
        }

        m_size -= diff;
    }
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Refit()
{
    if (m_capacity == Size()) {
        return;
    }

    SetCapacity(Size());
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PushBack(const ValueType &value) -> ValueType&
{
    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    // set item at index
    Memory::Construct<T>(&GetStorage()[m_size++].data_buffer, value);

    return Back();
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PushBack(ValueType &&value) -> ValueType&
{
    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

    // set item at index
    Memory::Construct<T>(&GetStorage()[m_size++].data_buffer, std::forward<ValueType>(value));

    return Back();
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PushFront(const ValueType &value) -> ValueType&
{
    if (m_start_offset == 0) {
        // have to push everything else over by 1
        if (m_size + push_front_padding >= m_capacity) {
            SetCapacity(
                GetCapacity(Size() + push_front_padding),
                push_front_padding // copy_offset is 1 so we have a space for 1 at the start
            );
        } else {
            auto *buffer = GetStorage();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;) {
                --index;

                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&buffer[move_index].data_buffer, std::move(buffer[index].Get()));
                } else {
                    Memory::Construct<T>(&buffer[move_index].data_buffer, buffer[index].Get());
                }

                // manual destructor call
                Memory::Destruct(buffer[index].Get());
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    Memory::Construct<T>(&GetStorage()[m_start_offset].data_buffer, value);

    return Front();
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PushFront(ValueType &&value) -> ValueType&
{
    if (m_start_offset == 0) {
        // have to push everything else over by 1
        if (m_size + push_front_padding >= m_capacity) {
            SetCapacity(
                GetCapacity(Size() + push_front_padding),
                push_front_padding // copy_offset is 1 so we have a space for 1 at the start
            );
        } else {
            auto *buffer = GetStorage();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;) {
                --index;

                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&buffer[move_index].data_buffer, std::move(buffer[index].Get()));
                } else {
                    Memory::Construct<T>(&buffer[move_index].data_buffer, buffer[index].Get());
                }

                // manual destructor call
                Memory::Destruct(buffer[index].Get());
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    Memory::Construct<T>(&GetStorage()[m_start_offset].data_buffer, std::move(value));

    return Front();
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Concat(const Array &other)
{
    if (m_size + other.Size() >= m_capacity) {
        if (m_capacity >= Size() + other.Size()) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + other.Size()));
        }
    }

    auto *buffer = GetStorage();

    for (SizeType i = 0; i < other.Size(); ++i) {
        // set item at index
        Memory::Construct<T>(&buffer[m_size++].data_buffer, other[i]);
    }
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Concat(Array &&other)
{
    if (m_size + other.Size() >= m_capacity) {
        if (m_capacity >= Size() + other.Size()) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + other.Size()));
        }
    }

    auto *buffer = GetStorage();

    for (SizeType i = 0; i < other.Size(); ++i) {
        // set item at index
        Memory::Construct<T>(&buffer[m_size++].data_buffer, std::move(other[i]));
    }

    other.Clear();
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Shift(SizeType count)
{
    SizeType new_size = 0;

    auto *buffer = GetStorage();

    for (SizeType index = m_start_offset; index < m_size; ++index, ++new_size) {
        if (index + count >= m_size) {
            break;
        }

        if constexpr (std::is_move_assignable_v<T>) {
            buffer[index].Get() = std::move(buffer[index + count].Get());
        } else if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(buffer[index].Get());
            Memory::Construct<T>(&buffer[index].data_buffer, std::move(buffer[index + count].Get()));
        } else {
            buffer[index].Get() = buffer[index + count].Get();
        }

        // manual destructor call
        Memory::Destruct(buffer[index + count].Get());
    }

    m_size = new_size;
}


template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes> Array<T, NumInlineBytes>::Slice(int first, int last) const
{
    if (first < 0) {
        first = Size() + first;
    }

    if (last < 0) {
        last = Size() + last;
    }

    if (first < 0) {
        first = 0;
    }

    if (last < 0) {
        last = 0;
    }

    if (first > last) {
        return Array<T, NumInlineBytes>();
    }

    if (first >= Size()) {
        return Array<T, NumInlineBytes>();
    }

    if (last >= Size()) {
        last = Size() - 1;
    }

    Array<T, NumInlineBytes> result;
    result.ResizeUninitialized(last - first + 1);

    auto *buffer = GetStorage();
    auto *result_buffer = result.GetStorage();

    for (SizeType i = 0; i < result.m_size; ++i) {
        Memory::Construct<T>(&result_buffer[i].data_buffer, buffer[first + i].Get());
    }

    return result;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::Erase(ConstIterator iter) -> Iterator
{
    const Iterator begin = Begin();
    const Iterator end = End();
    const SizeType size_offset = Size();

    if (iter < begin || iter >= end) {
        return end;
    }

    const SizeType dist = iter - begin;

    auto *buffer = GetStorage();

    for (SizeType index = dist; index < size_offset - 1; ++index) {
        if constexpr (std::is_move_assignable_v<T>) {
            buffer[m_start_offset + index].Get() = std::move(buffer[m_start_offset + index + 1].Get());
        } else if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(buffer[m_start_offset + index].Get());
            Memory::Construct<T>(&buffer[m_start_offset + index].data_buffer, std::move(buffer[m_start_offset + index + 1].Get()));
        } else {
            buffer[m_start_offset + index].Get() = buffer[m_start_offset + index + 1].Get();
        }
    }

    Memory::Destruct(buffer[m_size - 1].Get());
    --m_size;
    
    return begin + dist;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::Erase(const T &value) -> Iterator
{
    ConstIterator iter = Base::Find(value);

    if (iter != End()) {
        return Erase(iter);
    }

    return End();
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::EraseAt(typename Array::Base::KeyType index) -> Iterator
{
    return Erase(Begin() + index);
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::Insert(ConstIterator where, const ValueType &value) -> Iterator
{
    const SizeType dist = where - Begin();

    if (where == End()) {
        PushBack(std::move(value));
        
        return &GetBuffer()[m_size - 1];
    } else if (where == Begin() && dist <= m_start_offset) {
        PushFront(std::move(value));

        return Begin();
    }

#ifdef HYP_DEBUG_MODE
    AssertThrow(where >= Begin() && where <= End());
#endif

    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

#ifdef HYP_DEBUG_MODE
    AssertThrow(m_capacity >= m_size + 1);
#endif

    SizeType index;

    auto *buffer = GetStorage();

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, std::move(buffer[index + m_start_offset - 1].Get()));
        } else {
            Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, buffer[index + m_start_offset - 1].Get());
        }

        Memory::Destruct(buffer[index + m_start_offset - 1].Get());
    }

    Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, value);

    ++m_size;

    return Begin() + index;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::Insert(ConstIterator where, ValueType &&value) -> Iterator
{
    const SizeType dist = where - Begin();

    if (where == End()) {
        PushBack(std::move(value));
        
        return &GetBuffer()[m_size - 1];
    } else if (where == Begin() && dist <= m_start_offset) {
        PushFront(std::move(value));

        return Begin();
    }

#ifdef HYP_DEBUG_MODE
    AssertThrow(where >= Begin() && where <= End());
#endif

    if (m_size + 1 >= m_capacity) {
        if (m_capacity >= Size() + 1) {
            ResetOffsets();
        } else {
            SetCapacity(GetCapacity(Size() + 1));
        }
    }

#ifdef HYP_DEBUG_MODE
    AssertThrow(m_capacity >= m_size + 1);
#endif

    SizeType index;

    auto *buffer = GetStorage();

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, std::move(buffer[index + m_start_offset - 1].Get()));
        } else {
            Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, buffer[index + m_start_offset - 1].Get());
        }

        Memory::Destruct(buffer[index + m_start_offset - 1].Get());
    }

    Memory::Construct<T>(&buffer[index + m_start_offset].data_buffer, std::move(value));

    ++m_size;

    return Begin() + index;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PopFront() -> ValueType
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(Size() != 0);
#endif

    auto value = std::move(GetStorage()[m_start_offset].Get());

    Memory::Destruct(GetStorage()[m_start_offset].Get());

    ++m_start_offset;

    return value;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PopBack() -> ValueType
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(m_size != 0);
#endif

    auto value = std::move(GetStorage()[m_size - 1].Get());

    Memory::Destruct(GetStorage()[m_size - 1].Get());

    --m_size;

    return value;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Clear()
{
    auto *buffer = GetStorage();

    while (m_size - m_start_offset) {
        // manual destructor call
        Memory::Destruct(buffer[m_size - 1].Get());
        --m_size;
    }

    m_size = 0;
    m_start_offset = 0;

    //Refit();
}

} // namespace containers

template <class T, SizeType NumInlineBytes = 256u>
using Array = containers::Array<T, NumInlineBytes>;

// traits
template <class T>
struct IsArray { enum { value = false }; };

template <class T, SizeType NumInlineBytes>
struct IsArray<Array<T, NumInlineBytes>> { enum { value = true }; };

} // namespace hyperion

#endif