/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARRAY_HPP
#define HYPERION_ARRAY_HPP

#include <core/containers/ContainerBase.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/utilities/Pair.hpp>
#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Span.hpp>
#include <core/memory/Memory.hpp>
#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <algorithm>
#include <utility>
#include <cmath>

namespace hyperion {

namespace containers {


namespace detail {

template <class T, SizeType NumInlineBytes, class T2 = void>
struct ArrayStorage;

template <class T>
struct ArrayStorage<T, 0>
{
    static constexpr bool use_inline_storage = false;
    static constexpr SizeType num_inline_bytes = 0;
    static constexpr SizeType num_inline_elements = 0;

    void *m_buffer = nullptr;
    
    HYP_FORCE_INLINE T *GetBuffer()
    {
        return reinterpret_cast<T *>(m_buffer);
    }

    HYP_FORCE_INLINE const T *GetBuffer() const
    {
        return reinterpret_cast<const T *>(m_buffer);
    }

    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_buffer != nullptr;
    }

    HYP_FORCE_INLINE void AllocateDynamic(SizeType size)
    {
        AssertThrow(m_buffer == nullptr);

        AssertThrow(size <= SIZE_MAX / sizeof(T));

        m_buffer = static_cast<T *>(Memory::Allocate(sizeof(T) * size));
        AssertThrow(m_buffer != nullptr);
    }

    HYP_FORCE_INLINE void FreeDynamic()
    {
        AssertThrow(m_buffer != nullptr);

        Memory::Free(m_buffer);
        m_buffer = nullptr;
    }
};

template <class T, SizeType NumInlineBytes>
struct ArrayStorage<T, NumInlineBytes, std::enable_if_t< (sizeof(T) <= NumInlineBytes) > >
{
    static constexpr bool use_inline_storage = true;
    static constexpr SizeType num_inline_bytes = NumInlineBytes;
    static constexpr SizeType num_inline_elements = (num_inline_bytes / sizeof(T));
    
    union {
        void                                        *m_buffer;
        ValueStorageArray<T, num_inline_elements>   m_inline_storage;
    };

    bool                                            m_is_dynamic = false;
    
    HYP_FORCE_INLINE T *GetBuffer()
    {
        return reinterpret_cast<T *>(m_is_dynamic ? m_buffer : &m_inline_storage[0]);
    }

    HYP_FORCE_INLINE const T *GetBuffer() const
    {
        return reinterpret_cast<const T *>(m_is_dynamic ? m_buffer : &m_inline_storage[0]);
    }

    HYP_FORCE_INLINE bool IsDynamic() const
    {
        return m_is_dynamic;
    }

    HYP_FORCE_INLINE void AllocateDynamic(SizeType size)
    {
        AssertThrow(!m_is_dynamic);

        AssertThrow(size <= SIZE_MAX / sizeof(T));

        m_buffer = static_cast<T *>(Memory::Allocate(sizeof(T) * size));
        AssertThrow(m_buffer != nullptr);

        m_is_dynamic = true;
    }

    HYP_FORCE_INLINE void FreeDynamic()
    {
        AssertThrow(m_is_dynamic);

        Memory::Free(m_buffer);
        m_buffer = nullptr;

        m_is_dynamic = false;
    }
};

template <class T, SizeType NumInlineBytes>
struct ArrayStorage<T, NumInlineBytes, std::enable_if_t< NumInlineBytes != 0 && (sizeof(T) > NumInlineBytes) > > : ArrayStorage<T, 0>
{

};

} // namespace detail

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

    using ArrayStorageType = detail::ArrayStorage<T, NumInlineBytes>;

    static constexpr bool is_contiguous = true;
    
    static constexpr bool use_inline_storage = ArrayStorageType::use_inline_storage;
    static constexpr SizeType num_inline_bytes = ArrayStorageType::num_inline_bytes;
    static constexpr SizeType num_inline_elements = ArrayStorageType::num_inline_elements;

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

    Array(Span<T> span)
        : Array(span.Data(), span.Size())
    {
    }

    Array(Span<const T> span)
        : Array(span.Data(), span.Size())
    {
    }

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


    template <SizeType OtherNumInlineBytes>
    Array(const Array<T, OtherNumInlineBytes> &other)
        : Array()
    {
        const SizeType size = other.Size();

        SetCapacity(size);

        T *buffer = GetBuffer();

        for (SizeType i = 0; i < size; i++) {
            Memory::Construct<T>(&buffer[m_size++], other.Data()[i]);
        }
    }

    Array(Array &&other) noexcept;

    template <SizeType OtherNumInlineBytes>
    Array(Array<T, OtherNumInlineBytes> &&other)
        : Array()
    {
        const SizeType size = other.Size();

        SetCapacity(size);

        T *buffer = GetBuffer();

        for (SizeType i = 0; i < size; i++) {
            Memory::Construct<T>(&buffer[m_size++], std::move(other.Data()[i]));
        }

        other.Clear();
    }

    ~Array();

    Array &operator=(const Array &other);
    Array &operator=(Array &&other) noexcept;

    /*! \brief Returns the number of elements in the array. */
    HYP_FORCE_INLINE SizeType Size() const
        { return m_size - m_start_offset; }

    /*! \brief Returns the size in bytes of the array. */
    HYP_FORCE_INLINE SizeType ByteSize() const
        { return (m_size - m_start_offset) * sizeof(T); }

    /*! \brief Returns a pointer to the first element in the array. */
    HYP_FORCE_INLINE ValueType *Data()
        { return &GetBuffer()[m_start_offset]; }

    /*! \brief Returns a pointer to the first element in the array. */
    HYP_FORCE_INLINE const ValueType *Data() const
        { return &GetBuffer()[m_start_offset]; }

    /*! \brief Returns a reference to the first element in the array. */
    HYP_FORCE_INLINE ValueType &Front()
        { return GetBuffer()[m_start_offset]; }
    
    /*! \brief Returns a reference to the first element in the array. */
    HYP_FORCE_INLINE const ValueType &Front() const
        { return GetBuffer()[m_start_offset]; }

    /*! \brief Returns a reference to the last element in the array.  */
    HYP_FORCE_INLINE ValueType &Back()
        { return GetBuffer()[m_size - 1]; }

    /*! \brief Returns a reference to the last element in the array. */
    HYP_FORCE_INLINE const ValueType &Back() const
        { return GetBuffer()[m_size - 1]; }

    /*! \brief Returns true if the array has no elements. */
    HYP_FORCE_INLINE bool Empty() const
        { return Size() == 0; }

    /*! \brief Returns true if the array has any elements. */
    HYP_FORCE_INLINE bool Any() const
        { return Size() != 0; }

    /*! \brief Returns the element at the given index. No bounds checking is performed. */
    HYP_FORCE_INLINE ValueType &operator[](KeyType index)
        { return GetBuffer()[m_start_offset + index]; }

    /*! \brief Returns the element at the given index. No bounds checking is performed. */
    HYP_FORCE_INLINE const ValueType &operator[](KeyType index) const
        { return GetBuffer()[m_start_offset + index]; }

    /*! \brief Reserves enough space for {capacity} elements. If the capacity is smaller than the current capacity, nothing happens. */
    void Reserve(SizeType capacity);

    /*! \brief Resizes the array to the given size. If the size is smaller than the current size, the array is truncated. */
    void Resize(SizeType new_size);

    /*! \brief Resizes the array to the given size without initializing the new elements. Memory::Construct must be manually called on the new elements. */
    void ResizeUninitialized(SizeType new_size);

    /*! \brief Refits the array to the smallest possible size. This is useful if you have a large array and want to free up memory. */
    void Refit();

    /*! \brief Updates the capacity of the array to be at least {capacity} */
    void SetCapacity(SizeType capacity, SizeType copy_offset = 0);

    HYP_FORCE_INLINE SizeType Capacity() const
        { return m_capacity; }

    /*! \brief Push an item to the back of the container.
     *  \param value The value to push back.
     *  \return Reference to the newly pushed back item. */
    ValueType &PushBack(const ValueType &value);

    /*! \brief Push an item to the back of the container.
     *  \param value The value to push back.
     *  \return Reference to the newly pushed back item. */
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

    /*! \brief Construct an item in place at the back of the array.
     *  \param args Arguments to forward to the constructor of the item.
     *  \return Reference to the newly constructed item. */
    template <class ... Args>
    ValueType &EmplaceBack(Args &&... args)
    {
        if (m_size + 1 >= m_capacity) {
            if (m_capacity >= Size() + 1) {
                ResetOffsets();
            } else {
                SetCapacity(GetCapacity(Size() + 1));
            }
        }

        // set item at index
        T *buffer = GetBuffer();
        T *element = &buffer[m_size++];

        Memory::Construct<T>(element, std::forward<Args>(args)...);

        return *element;
    }

    /*! \brief Construct an item in place at the front of the array.
     *  If there is no space at the front, the array is resized and all elements are shifted to the right.
     *  \param args Arguments to forward to the constructor of the item.
     *  \return Reference to the newly constructed item. */
    template <class ... Args>
    ValueType &EmplaceFront(Args &&... args)
    {
        if (m_start_offset == 0) {
            // have to push everything else over by 1
            if (m_size + push_front_padding >= m_capacity) {
                SetCapacity(
                    GetCapacity(Size() + push_front_padding),
                    push_front_padding // copy_offset is 1 so we have a space for 1 at the start
                );
            } else {
                T *buffer = GetBuffer();

                // shift over without realloc
                for (SizeType index = Size(); index > 0;) {
                    --index;

                    const auto move_index = index + push_front_padding;

                    Memory::Construct<T>(&buffer[move_index], std::forward<Args>(args)...);

                    // manual destructor call
                    Memory::Destruct(buffer[index]);
                }

                m_start_offset = push_front_padding;
                m_size += m_start_offset;
            }
        }

        --m_start_offset;

        T *buffer = GetBuffer();
        T *element = &buffer[m_start_offset];

        Memory::Construct<T>(element, std::forward<Args>(args)...);

        return *element;
    }

    /*! \brief Shift the array to the left by {count} times */
    void Shift(SizeType count);

    HYP_NODISCARD Array<T, NumInlineBytes> Slice(int first, int last) const;

    /*! \brief Modify the array by appending all items in \ref{other} to the current array. */
    void Concat(const Array &other);
    
    /*! \brief Modify the array by appending all items in \ref{other} to the current array.
     *  All items from the other array are moved over, thus \ref{other} will be empty after the call.
     */
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
    HYP_FORCE_INLINE bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    /*! \brief Returns true if all elements in the array satisfy the given lambda. */
    template <class Lambda>
    HYP_FORCE_INLINE bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    template <SizeType OtherNumInlineBytes>
    HYP_FORCE_INLINE bool operator==(const Array<T, OtherNumInlineBytes> &other) const
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
    HYP_FORCE_INLINE bool operator!=(const Array<T, OtherNumInlineBytes> &other) const
        { return !operator==(other); }

    /*! \brief Creates a Span<T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<T>()
        { return Span<T>(Data(), Size()); }

    /*! \brief Creates a Span<const T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<const T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const T>() const
        { return Span<const T>(Data(), Size()); }

    /*! \brief Creates a Span<T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<T> ToSpan()
        { return Span<T>(Data(), Size()); }

    /*! \brief Creates a Span<const T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<const T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<const T> ToSpan() const
        { return Span<const T>(Data(), Size()); }

    /*! \brief Returns a ByteView of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (offset >= Size()) {
            return ByteView();
        }

        if (size > Size()) {
            size = Size();
        }

        return ByteView(reinterpret_cast<ubyte *>(Data()) + offset, size);
    }

    /*! \brief Returns a ConstByteView of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (offset >= Size()) {
            return ConstByteView();
        }

        if (size > Size()) {
            size = Size();
        }

        return ConstByteView(reinterpret_cast<const ubyte *>(Data()) + offset, size);
    }
    
    HYP_DEF_STL_BEGIN_END(
        GetBuffer() + m_start_offset,
        GetBuffer() + m_size
    )

protected:
    HYP_FORCE_INLINE T *GetBuffer()
    {
        return m_storage.GetBuffer();
    }

    HYP_FORCE_INLINE const T *GetBuffer() const
    {
        return m_storage.GetBuffer();
    }

    void ResetOffsets();

    auto RealBegin() const { return GetBuffer(); }
    auto RealEnd() const { return GetBuffer() + m_size; }

    static SizeType GetCapacity(SizeType size)
    {
        return 1ull << static_cast<SizeType>(std::ceil(std::log(size) / std::log(2.0)));
    }

    SizeType m_size;
    SizeType m_capacity;

    // static_assert(sizeof(Storage) == sizeof(Storage::data_buffer), "Storage struct should not be padded");
    // static_assert(alignof(Storage) == alignof(Storage::data_buffer), "Storage struct should not be padded");

protected:
    SizeType            m_start_offset;

    ArrayStorageType    m_storage;
};

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array()
    : m_size(0),
      m_capacity(num_inline_elements),
      m_start_offset(0)
{
#ifdef HYP_DEBUG_MODE
    if constexpr (use_inline_storage) {
        Memory::Garble(&m_storage.m_inline_storage[0], num_inline_elements * sizeof(T));
    }
#endif
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array(const Array &other)
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_start_offset(other.m_start_offset)
{
    if (other.m_storage.IsDynamic()) {
        m_storage.AllocateDynamic(m_capacity);
    }

    T *buffer = GetBuffer();

    // copy all members
    for (SizeType i = m_start_offset; i < m_size; ++i) {
        Memory::Construct<T>(&buffer[i], other.GetBuffer()[i]);
    }
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::Array(Array &&other) noexcept
    : m_size(other.m_size),
      m_capacity(other.m_capacity),
      m_start_offset(other.m_start_offset)
{
    if (other.m_storage.IsDynamic()) {
        m_storage.m_buffer = other.m_storage.m_buffer;
        other.m_storage.m_buffer = nullptr;

        if constexpr (use_inline_storage) {
            m_storage.m_is_dynamic = true;
            other.m_storage.m_is_dynamic = false;
        }
    } else {
        T *buffer = GetBuffer();

        // move all members
        for (SizeType i = m_start_offset; i < m_size; ++i) {
            Memory::Construct<T>(&buffer[i], std::move(other.GetBuffer()[i]));
        }
    }

    other.m_size = 0;
    other.m_capacity = num_inline_elements;
    other.m_start_offset = 0;

#ifdef HYP_DEBUG_MODE
    if constexpr (use_inline_storage) {
        Memory::Garble(&other.m_storage.m_inline_storage[0], num_inline_elements * sizeof(T));
    }
#endif
}

template <class T, SizeType NumInlineBytes>
Array<T, NumInlineBytes>::~Array()
{
    T *buffer = GetBuffer();

    for (SizeType i = m_size; i > m_start_offset;) {
        Memory::Destruct(buffer[--i]);
    }
    
    if (m_storage.IsDynamic()) {
        m_storage.FreeDynamic();
    }
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::operator=(const Array &other) -> Array&
{
    if (this == std::addressof(other)) {
        return *this;
    }

    T *buffer = GetBuffer();
    const T *other_buffer = other.GetBuffer();

    if (m_capacity < other.m_capacity) {
        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(buffer[--i]);
        }

        if (m_storage.IsDynamic()) {
            m_storage.FreeDynamic();
        }

        if (other.m_storage.IsDynamic()) {
            m_capacity = other.m_capacity;

            m_storage.AllocateDynamic(m_capacity);
        } else {
            m_capacity = num_inline_elements;
        }

        m_size = other.m_size;
        m_start_offset = other.m_start_offset;

        buffer = GetBuffer();

        // copy all objects
        for (SizeType i = m_start_offset; i < m_size; ++i) {
            Memory::Construct<T>(&buffer[i], other_buffer[i]);
        }
    } else {
        const SizeType current_size = Size();
        const SizeType other_size = other.Size();

        for (SizeType i = m_start_offset; i < m_size; i++) {
            Memory::Destruct(buffer[i]);
        }

        for (SizeType i = 0; i < other_size; i++) {
            Memory::Construct<T>(&buffer[i], other_buffer[other.m_start_offset + i]);
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
        T *buffer = GetBuffer();

        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(buffer[--i]);
        }
    }

    if (m_storage.IsDynamic()) {
        m_storage.FreeDynamic();
    }
    
    if (other.m_storage.IsDynamic()) {
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        m_start_offset = other.m_start_offset;
        
        m_storage.m_buffer = other.m_storage.m_buffer;

        if constexpr (use_inline_storage) {
            m_storage.m_is_dynamic = true;
        }
    } else {
        m_capacity = num_inline_elements;

        if constexpr (use_inline_storage) {
            m_storage.m_is_dynamic = false;

            // move items individually
            for (SizeType i = 0; i < other.Size(); ++i) {
                Memory::Construct<T>(&m_storage.m_inline_storage[i].data_buffer, std::move(other.m_storage.m_inline_storage[other.m_start_offset + i].Get()));
            }
        }

        m_size = other.Size();
        m_start_offset = 0;
        
        if constexpr (use_inline_storage) {
            // manually call destructors
            for (SizeType i = other.m_size; i > other.m_start_offset;) {
                Memory::Destruct(other.m_storage.m_inline_storage[--i].Get());
            }
        }
    }

    other.m_size = 0;
    other.m_capacity = num_inline_elements;
    other.m_start_offset = 0;
    
    other.m_storage.m_buffer = nullptr;

    if constexpr (use_inline_storage) {
        other.m_storage.m_is_dynamic = false;
    }

    return *this;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::ResetOffsets()
{
    if (m_start_offset == 0) {
        return;
    }

    T *buffer = GetBuffer();

    // shift all items to left
    for (SizeType index = m_start_offset; index < m_size; ++index) {
        const auto move_index = index - m_start_offset;

        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[move_index], std::move(buffer[index]));
        } else {
            Memory::Construct<T>(&buffer[move_index], buffer[index]);
        }

        // manual destructor call
        Memory::Destruct(buffer[index]);
    }

    m_size -= m_start_offset;
    m_start_offset = 0;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::SetCapacity(SizeType capacity, SizeType copy_offset)
{
    T *old_buffer = GetBuffer();

    if (capacity > num_inline_elements) {
        AssertThrow(capacity <= SIZE_MAX / sizeof(T));

        // delete and copy all over again
        T *new_buffer = static_cast<T *>(Memory::Allocate(sizeof(T) * capacity));
        AssertThrow(new_buffer != nullptr);

        // AssertThrow(Size() <= m_capacity);
        
        for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
            if constexpr (std::is_move_constructible_v<T>) {
                Memory::Construct<T>(&new_buffer[i], std::move(old_buffer[j]));
            } else {
                Memory::Construct<T>(&new_buffer[i], old_buffer[j]);
            }
        }

        // manually call destructors of old buffer
        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(old_buffer[--i]);
        }

        if (m_storage.IsDynamic()) {
            // delete old buffer memory
            Memory::Free(old_buffer);
        }

        // set internal buffer to the new one
        m_capacity = capacity;
        m_size -= static_cast<int64>(m_start_offset) - static_cast<int64>(copy_offset);
        m_start_offset = copy_offset;
        
        m_storage.m_buffer = new_buffer;

        if constexpr (use_inline_storage) {
            m_storage.m_is_dynamic = true;
        }
    } else {
        if (m_storage.IsDynamic()) { // switch from dynamic to non-dynamic
            if constexpr (use_inline_storage) {
                for (SizeType i = copy_offset, j = m_start_offset; j < m_size; ++i, ++j) {
                    m_storage.m_inline_storage[i].Get() = std::move(old_buffer[j]);
                }
            }

            // call destructors on old buffer
            for (SizeType i = m_size; i > m_start_offset;) {
                Memory::Destruct(old_buffer[--i]);
            }

            Memory::Free(old_buffer);

            m_capacity = num_inline_elements;

            m_storage.m_buffer = nullptr;
            
            if constexpr (use_inline_storage) {
                m_storage.m_is_dynamic = false;
            }
        } else if (m_start_offset != copy_offset) {
            if constexpr (use_inline_storage) {
                if (m_start_offset > copy_offset) {
                    const SizeType diff = m_start_offset - copy_offset;

                    // shift left
                    for (SizeType index = m_start_offset; index < m_size; ++index) {
                        const SizeType move_index = index - diff;

                        if constexpr (std::is_move_constructible_v<T>) {
                            Memory::Construct<T>(&m_storage.m_inline_storage[move_index].data_buffer, std::move(m_storage.m_inline_storage[index].Get()));
                        } else {
                            Memory::Construct<T>(&m_storage.m_inline_storage[move_index].data_buffer, m_storage.m_inline_storage[index].Get());
                        }

                        // manual destructor call
                        Memory::Destruct(m_storage.m_inline_storage[index].Get());
                    }
                } else {
                    // shift right
                    const SizeType diff = copy_offset - m_start_offset;

                    for (SizeType index = m_size; index > m_start_offset;) {
                        --index;

                        const SizeType move_index = index + diff;

                        if constexpr (std::is_move_constructible_v<T>) {
                            Memory::Construct<T>(&m_storage.m_inline_storage[move_index].data_buffer, std::move(m_storage.m_inline_storage[index].Get()));
                        } else {
                            Memory::Construct<T>(&m_storage.m_inline_storage[move_index].data_buffer, m_storage.m_inline_storage[index].Get());
                        }

                        // manual destructor call
                        Memory::Destruct(m_storage.m_inline_storage[index].Get());
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

        T *buffer = GetBuffer();

        if constexpr (std::is_fundamental_v<T> || std::is_trivially_constructible_v<T>) {
            Memory::MemSet(&buffer[m_size], 0, sizeof(T) * diff);

            m_size += diff;
        } else {
            while (Size() < new_size) {
                // construct item at index
                Memory::Construct<T>(&buffer[m_size++]);
            }
        }
    } else {
        const SizeType diff = current_size - new_size;

        for (SizeType i = m_size; i > m_start_offset;) {
            Memory::Destruct(GetBuffer()[--i]);
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
            Memory::Destruct(GetBuffer()[--i]);
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
    T *buffer = GetBuffer();
    T *element = &buffer[m_size++];

    Memory::Construct<T>(element, value);

    return *element;
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
    T *buffer = GetBuffer();
    T *element = &buffer[m_size++];

    Memory::Construct<T>(element, std::move(value));

    return *element;
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
            T *buffer = GetBuffer();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;) {
                --index;

                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&buffer[move_index], std::move(buffer[index]));
                } else {
                    Memory::Construct<T>(&buffer[move_index], buffer[index]);
                }

                // manual destructor call
                Memory::Destruct(buffer[index]);
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    Memory::Construct<T>(&GetBuffer()[m_start_offset], value);

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
            T *buffer = GetBuffer();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;) {
                --index;

                const auto move_index = index + push_front_padding;

                if constexpr (std::is_move_constructible_v<T>) {
                    Memory::Construct<T>(&buffer[move_index], std::move(buffer[index]));
                } else {
                    Memory::Construct<T>(&buffer[move_index], buffer[index]);
                }

                // manual destructor call
                Memory::Destruct(buffer[index]);
            }

            m_start_offset = push_front_padding;
            m_size += m_start_offset;
        }
    }

    // in-place
    --m_start_offset;

    T *buffer = GetBuffer();
    T *element = &buffer[m_start_offset];

    Memory::Construct<T>(element, std::move(value));

    return *element;
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

    T *buffer = GetBuffer();

    for (SizeType i = 0; i < other.Size(); ++i) {
        // set item at index
        Memory::Construct<T>(&buffer[m_size++], other[i]);
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

    T *buffer = GetBuffer();

    for (SizeType i = 0; i < other.Size(); ++i) {
        // set item at index
        Memory::Construct<T>(&buffer[m_size++], std::move(other[i]));
    }

    other.Clear();
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Shift(SizeType count)
{
    SizeType new_size = 0;

    T *buffer = GetBuffer();

    for (SizeType index = m_start_offset; index < m_size; ++index, ++new_size) {
        if (index + count >= m_size) {
            break;
        }

        if constexpr (std::is_move_assignable_v<T>) {
            buffer[index] = std::move(buffer[index + count]);
        } else if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(buffer[index]);
            Memory::Construct<T>(&buffer[index], std::move(buffer[index + count]));
        } else {
            buffer[index] = buffer[index + count];
        }

        // manual destructor call
        Memory::Destruct(buffer[index + count]);
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

    const T *buffer = GetBuffer();
    T *result_buffer = result.GetBuffer();

    for (SizeType i = 0; i < result.m_size; ++i) {
        Memory::Construct<T>(&result_buffer[i], buffer[first + i]);
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

    T *buffer = GetBuffer();

    for (SizeType index = dist; index < size_offset - 1; ++index) {
        /*if constexpr (std::is_move_assignable_v<T>) {
            buffer[m_start_offset + index].Get() = std::move(buffer[m_start_offset + index + 1].Get());
        } else*/ if constexpr (std::is_move_constructible_v<T>) {
            Memory::Destruct(buffer[m_start_offset + index]);
            Memory::Construct<T>(&buffer[m_start_offset + index], std::move(buffer[m_start_offset + index + 1]));
        } else {
            Memory::Destruct(buffer[m_start_offset + index]);
            Memory::Construct<T>(&buffer[m_start_offset + index], buffer[m_start_offset + index + 1]);
            // buffer[m_start_offset + index].Get() = buffer[m_start_offset + index + 1].Get();
        }
    }

    Memory::Destruct(buffer[m_size - 1]);
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

    T *buffer = GetBuffer();

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[index + m_start_offset], std::move(buffer[index + m_start_offset - 1]));
        } else {
            Memory::Construct<T>(&buffer[index + m_start_offset], buffer[index + m_start_offset - 1]);
        }

        Memory::Destruct(buffer[index + m_start_offset - 1]);
    }

    Memory::Construct<T>(&buffer[index + m_start_offset], value);

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

    T *buffer = GetBuffer();

    for (index = Size(); index > dist; --index) {
        if constexpr (std::is_move_constructible_v<T>) {
            Memory::Construct<T>(&buffer[index + m_start_offset], std::move(buffer[index + m_start_offset - 1]));
        } else {
            Memory::Construct<T>(&buffer[index + m_start_offset], buffer[index + m_start_offset - 1]);
        }

        Memory::Destruct(buffer[index + m_start_offset - 1]);
    }

    Memory::Construct<T>(&buffer[index + m_start_offset], std::move(value));

    ++m_size;

    return Begin() + index;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PopFront() -> ValueType
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(Size() != 0);
#endif

    auto value = std::move(GetBuffer()[m_start_offset]);

    Memory::Destruct(GetBuffer()[m_start_offset]);

    ++m_start_offset;

    return value;
}

template <class T, SizeType NumInlineBytes>
auto Array<T, NumInlineBytes>::PopBack() -> ValueType
{
#ifdef HYP_DEBUG_MODE
    AssertThrow(m_size != 0);
#endif

    auto value = std::move(GetBuffer()[m_size - 1]);

    Memory::Destruct(GetBuffer()[m_size - 1]);

    --m_size;

    return value;
}

template <class T, SizeType NumInlineBytes>
void Array<T, NumInlineBytes>::Clear()
{
    T *buffer = GetBuffer();

    while (m_size - m_start_offset) {
        // manual destructor call
        Memory::Destruct(buffer[m_size - 1]);
        --m_size;
    }

    m_size = 0;
    m_start_offset = 0;

    //Refit();
}

/*! \brief A map function that applies a function to each element in a container.
 *  \param container The container to map over.
 *  \param func The function to apply to each element.
 *  \return A new array with the results of the function applied to each element. */
template <class ContainerType, class Function, typename = std::enable_if_t<!std::is_member_pointer_v<NormalizedType<Function>> && !std::is_member_function_pointer_v<NormalizedType<Function>>>>
auto Map(ContainerType &&container, Function &&func)
{
    Array<decltype(std::declval<NormalizedType<Function>>()(std::declval<typename NormalizedType<ContainerType>::ValueType>()))> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack(func(*it));
    }

    return result;
}

template <class ContainerType, class ResultType>
auto Map(ContainerType &&container, ResultType(NormalizedType<ContainerType>::ValueType::*MemFn)())
{
    Array<ResultType> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack(((*it).*MemFn)());
    }

    return result;
}

template <class ContainerType, class ResultType>
auto Map(ContainerType &&container, ResultType(NormalizedType<ContainerType>::ValueType::*MemFn)() const)
{
    Array<ResultType> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack(((*it).*MemFn)());
    }

    return result;
}

template <class ContainerType, class ResultType>
auto Map(ContainerType &&container, ResultType NormalizedType<ContainerType>::ValueType::*member)
{
    Array<ResultType> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack((*it).*member);
    }

    return result;
}

template <class ContainerType, class ResultType>
auto Map(ContainerType &&container, ResultType const NormalizedType<ContainerType>::ValueType::*member)
{
    Array<ResultType> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it) {
        result.PushBack((*it).*member);
    }

    return result;
}

} // namespace containers

template <class T, SizeType NumInlineBytes = 256u>
using Array = containers::Array<T, NumInlineBytes>;

using containers::Map;

// traits
template <class T>
struct IsArray { enum { value = false }; };

template <class T, SizeType NumInlineBytes>
struct IsArray<Array<T, NumInlineBytes>> { enum { value = true }; };

} // namespace hyperion

#endif