/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/ContainerBase.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Span.hpp>

#include <core/memory/allocator/Allocator.hpp>

#include <core/memory/Memory.hpp>

#include <core/debug/Debug.hpp>

#include <core/Defines.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <algorithm>
#include <utility>
#include <cmath>

namespace hyperion {

namespace containers {

template <class T, SizeType MaxInlineCapacityBytes = 256, class T2 = void>
struct ArrayDefaultAllocatorSelector;

template <class T, SizeType MaxInlineCapacityBytes>
struct ArrayDefaultAllocatorSelector<T, MaxInlineCapacityBytes, std::enable_if_t<!implementationExists<T>>>
{
    using Type = DynamicAllocator;
};

template <class T, SizeType MaxInlineCapacityBytes>
struct ArrayDefaultAllocatorSelector<T, MaxInlineCapacityBytes, std::enable_if_t<implementationExists<T> && (sizeof(T) <= MaxInlineCapacityBytes)>>
{
    using Type = InlineAllocator<MaxInlineCapacityBytes / sizeof(T)>;
};

template <class T, SizeType MaxInlineCapacityBytes>
struct ArrayDefaultAllocatorSelector<T, MaxInlineCapacityBytes, std::enable_if_t<implementationExists<T> && (sizeof(T) > MaxInlineCapacityBytes)>>
{
    using Type = DynamicAllocator;
};

/*! \brief Array class with smart front removal and inline storage so small arrays do not require any heap allocation
    \details Average speed is about the same as std::vector in most cases
    \note will use a bit more memory than std::vector, partially because of inline storage in the class,
    and partially due to the front offset member, used to have fewer deallocations/shifting on PopFront(). */
template <class T, class AllocatorType = typename ArrayDefaultAllocatorSelector<T>::Type>
class Array : public ContainerBase<Array<T, AllocatorType>, SizeType>
{
public:
    using Base = ContainerBase<Array<T, AllocatorType>, SizeType>;
    using KeyType = typename Base::KeyType;
    using ValueType = T;

    static constexpr bool isContiguous = true;

    // Allow other Array types to access private members
    template <class OtherT, class OtherAllocatorType>
    friend class Array;

protected:
    // on PushFront() we can pad the start with this number,
    // so when multiple successive calls to PushFront() happen,
    // we're not realloc'ing everything each time
    static constexpr SizeType pushFrontPadding = 4;

public:
    using Iterator = T*;
    using ConstIterator = const T*;
    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    Array();

    explicit Array(SizeType size)
        : Array()
    {
        Resize(size);
    }

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

        auto* storagePtr = Data();

        for (SizeType i = 0; i < Sz; ++i)
        {
            Memory::Construct<T>(storagePtr++, items[i]);
        }
    }

    template <SizeType Sz>
    Array(T (&&items)[Sz])
        : Array()
    {
        ResizeUninitialized(Sz);

        auto* storagePtr = Data();

        for (SizeType i = 0; i < Sz; ++i)
        {
            Memory::Construct<T>(storagePtr++, std::move(items[i]));
        }
    }

    template <SizeType Sz>
    Array(const FixedArray<T, Sz>& items)
        : Array(items.Begin(), items.End())
    {
    }

    template <SizeType Sz>
    Array(FixedArray<T, Sz>&& items)
        : Array()
    {
        ResizeUninitialized(Sz);

        auto* storagePtr = Data();

        for (SizeType i = 0; i < Sz; ++i)
        {
            Memory::Construct<T>(storagePtr++, std::move(items[i]));
        }
    }

    Array(T* ptr, SizeType size)
        : Array()
    {
        ResizeUninitialized(size);

        auto* storagePtr = Data();

        const T* first = ptr;
        const T* last = ptr + size;

        for (auto it = first; it != last; ++it)
        {
            Memory::Construct<T>(storagePtr++, *it);
        }
    }

    Array(Iterator first, Iterator last)
        : Array()
    {
        const SizeType dist = last - first;
        ResizeUninitialized(dist);

        auto* storagePtr = Data();

        for (auto it = first; it != last; ++it)
        {
            Memory::Construct<T>(storagePtr++, *it);
        }
    }

    Array(ConstIterator first, ConstIterator last)
        : Array()
    {
        const SizeType dist = last - first;
        ResizeUninitialized(dist);

        auto* storagePtr = Data();

        for (auto it = first; it != last; ++it)
        {
            Memory::Construct<T>(storagePtr++, *it);
        }
    }

    Array(const T* ptr, SizeType size)
        : Array(ptr, ptr + size)
    {
    }

    Array(std::initializer_list<T> initializerList)
        : Array(initializerList.begin(), initializerList.end())
    {
    }

    Array(const Array& other);

    template <class OtherAllocatorType, typename = std::enable_if_t<!std::is_same_v<OtherAllocatorType, AllocatorType>>>
    Array(const Array<T, OtherAllocatorType>& other)
        : Array()
    {
        m_size = other.Size();

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeCopy(other.Begin(), other.End());
    }

    Array(Array&& other) noexcept;

    template <class OtherAllocatorType, typename = std::enable_if_t<!std::is_same_v<OtherAllocatorType, AllocatorType>>>
    Array(Array<T, OtherAllocatorType>&& other)
        : Array()
    {
        if (other.m_allocation.IsDynamic())
        {
            m_size = other.m_size;
            m_startOffset = other.m_startOffset;

            m_allocation.TakeOwnership(other.GetBuffer(), other.GetBuffer() + other.m_size);

            other.m_allocation.SetToInitialState();

            other.m_size = 0;
            other.m_startOffset = 0;
        }
        else
        {
            m_size = other.m_size - other.m_startOffset;
            m_startOffset = 0;

            m_allocation.Allocate(m_size);
            m_allocation.InitFromRangeMove(other.Begin(), other.End());

            other.m_allocation.DestructInRange(other.m_startOffset, other.m_size);
            other.m_allocation.Free();

            other.m_size = 0;
            other.m_startOffset = 0;
        }
    }

    ~Array();

    Array& operator=(const Array& other);
    Array& operator=(Array&& other) noexcept;

    HYP_FORCE_INLINE typename AllocatorType::template Allocation<T>& GetAllocation()
    {
        return m_allocation;
    }

    HYP_FORCE_INLINE const typename AllocatorType::template Allocation<T>& GetAllocation() const
    {
        return m_allocation;
    }

    /*! \brief Returns the number of elements in the array. */
    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_size - m_startOffset;
    }

    /*! \brief Returns the size in bytes of the array. */
    HYP_FORCE_INLINE SizeType ByteSize() const
    {
        return (m_size - m_startOffset) * sizeof(T);
    }

    /*! \brief Returns a pointer to the first element in the array. */
    HYP_FORCE_INLINE ValueType* Data()
    {
        return &GetBuffer()[m_startOffset];
    }

    /*! \brief Returns a pointer to the first element in the array. */
    HYP_FORCE_INLINE const ValueType* Data() const
    {
        return &GetBuffer()[m_startOffset];
    }

    /*! \brief Returns a reference to the first element in the array. */
    HYP_FORCE_INLINE ValueType& Front()
    {
        return GetBuffer()[m_startOffset];
    }

    /*! \brief Returns a reference to the first element in the array. */
    HYP_FORCE_INLINE const ValueType& Front() const
    {
        return GetBuffer()[m_startOffset];
    }

    /*! \brief Returns a reference to the last element in the array.  */
    HYP_FORCE_INLINE ValueType& Back()
    {
        return GetBuffer()[m_size - 1];
    }

    /*! \brief Returns a reference to the last element in the array. */
    HYP_FORCE_INLINE const ValueType& Back() const
    {
        return GetBuffer()[m_size - 1];
    }

    /*! \brief Returns true if the array has no elements. */
    HYP_FORCE_INLINE bool Empty() const
    {
        return Size() == 0;
    }

    /*! \brief Returns true if the array has any elements. */
    HYP_FORCE_INLINE bool Any() const
    {
        return Size() != 0;
    }

    /*! \brief Returns the element at the given index. No bounds checking is performed in release mode. */
    HYP_FORCE_INLINE ValueType& operator[](KeyType index)
    {
        HYP_CORE_ASSERT(index >= 0 && index < Size(), "Index out of bounds");

        return GetBuffer()[m_startOffset + index];
    }

    /*! \brief Returns the element at the given index. No bounds checking is performed in release mode. */
    HYP_FORCE_INLINE const ValueType& operator[](KeyType index) const
    {
        HYP_CORE_ASSERT(index >= 0 && index < Size(), "Index out of bounds");

        return GetBuffer()[m_startOffset + index];
    }

    /*! \brief Reserves enough space for {capacity} elements. If the capacity is smaller than the current capacity, nothing happens. */
    void Reserve(SizeType capacity);

    /*! \brief Resizes the array to the given size. If the size is smaller than the current size, the array is truncated. */
    void Resize(SizeType newSize);

    /*! \brief Resizes the array to the given size without initializing the new elements. Memory::Construct must be manually called on the new elements. */
    void ResizeUninitialized(SizeType newSize);

    /*! \brief Resizes the array to the given size and sets the new elements to zero. This should be used for POD types only that do not require
     * constructor calls. */
    void ResizeZeroed(SizeType newSize);

    /*! \brief Refits the array to the smallest possible size. This is useful if you have a large array and want to free up memory. */
    void Refit();

    /*! \brief Updates the capacity of the array to be at least {capacity} */
    void SetCapacity(SizeType capacity, SizeType copyOffset = 0);

    HYP_FORCE_INLINE SizeType Capacity() const
    {
        return m_allocation.GetCapacity();
    }

    /*! \brief Push an item to the back of the container.
     *  \param value The value to push back.
     *  \return Reference to the newly pushed back item. */
    ValueType& PushBack(const ValueType& value);

    /*! \brief Push an item to the back of the container.
     *  \param value The value to push back.
     *  \return Reference to the newly pushed back item. */
    ValueType& PushBack(ValueType&& value);

    /*! \brief Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time.
        */
    ValueType& PushFront(const ValueType& value);

    /*! \brief Push an item to the front of the container.
        If any free spaces are available, they are used.
        Else, new space is allocated and all current elements are shifted to the right.
        Some padding is added so that successive calls to PushFront() do not incur an allocation
        each time. */
    ValueType& PushFront(ValueType&& value);

    /*! \brief Construct an item in place at the back of the array.
     *  \param args Arguments to forward to the constructor of the item.
     *  \return Reference to the newly constructed item. */
    template <class... Args>
    ValueType& EmplaceBack(Args&&... args)
    {
        if (m_size + 1 >= Capacity())
        {
            if (Capacity() >= Size() + 1)
            {
                ResetOffsets();
            }
            else
            {
                SetCapacity(CalculateDesiredCapacity(Size() + 1));
            }
        }

        // set item at index
        T* buffer = GetBuffer();
        T* element = &buffer[m_size++];

        Memory::Construct<T>(element, std::forward<Args>(args)...);

        return *element;
    }

    /*! \brief Construct an item in place at the front of the array.
     *  If there is no space at the front, the array is resized and all elements are shifted to the right.
     *  \param args Arguments to forward to the constructor of the item.
     *  \return Reference to the newly constructed item. */
    template <class... Args>
    ValueType& EmplaceFront(Args&&... args)
    {
        if (m_startOffset == 0)
        {
            // have to push everything else over by 1
            if (m_size + pushFrontPadding >= Capacity())
            {
                SetCapacity(
                    CalculateDesiredCapacity(Size() + pushFrontPadding),
                    pushFrontPadding // copyOffset is 1 so we have a space for 1 at the start
                );
            }
            else
            {
                T* buffer = GetBuffer();

                // shift over without realloc
                for (SizeType index = Size(); index > 0;)
                {
                    --index;

                    const auto moveIndex = index + pushFrontPadding;

                    Memory::Construct<T>(&buffer[moveIndex], std::forward<Args>(args)...);

                    // manual destructor call
                    Memory::Destruct(buffer[index]);
                }

                m_startOffset = pushFrontPadding;
                m_size += m_startOffset;
            }
        }

        --m_startOffset;

        T* buffer = GetBuffer();
        T* element = &buffer[m_startOffset];

        Memory::Construct<T>(element, std::forward<Args>(args)...);

        return *element;
    }

    /*! \brief Shift the array to the left by {count} times */
    void Shift(SizeType count);

    HYP_NODISCARD Array<T, AllocatorType> Slice(int first, int last) const;

    /*! \brief Modify the array by appending all items in \ref{other} to the current array. */
    void Concat(const Array& other);

    /*! \brief Modify the array by appending all items in \ref{other} to the current array.
     *  All items from the other array are moved over, thus \ref{other} will be empty after the call.
     */
    void Concat(Array&& other);

    /*! \brief Reverse the order of the elements in the array in place. */
    void Reverse();

    /*! \brief Build a new array with the elements in reverse order. Does not modify the original array. */
    template <class OtherAllocatorType>
    void Reverse(Array<T, OtherAllocatorType>& outArray) const
    {
        const SizeType size = Size();

        if (size < 2)
        {
            return;
        }

        outArray.ResizeUninitialized(size);

        T* buffer = GetBuffer();
        T* outBuffer = outArray.GetBuffer();

        for (SizeType i = 0; i < size; ++i)
        {
            Memory::Construct<T>(&outBuffer[i], buffer[size - 1 - i]);
        }
    }

    /*! \brief Erase an element by iterator. */
    Iterator Erase(ConstIterator iter);

    /*! \brief Erase an element by value. A Find() is performed, and if the result is not equal to End(),
     *  the element is removed. */
    Iterator Erase(const T& value);
    Iterator EraseAt(typename Base::KeyType index);
    Iterator Insert(ConstIterator where, const ValueType& value);
    Iterator Insert(ConstIterator where, ValueType&& value);

    ValueType PopFront();
    ValueType PopBack();

    void Clear();

    template <class OtherAllocatorType>
    HYP_FORCE_INLINE bool operator==(const Array<T, OtherAllocatorType>& other) const
    {
        if (std::addressof(other) == this)
        {
            return true;
        }

        if (Size() != other.Size())
        {
            return false;
        }

        if constexpr (std::is_fundamental_v<T> || std::is_enum_v<T>)
        {
            // if T is a fundamental type, we can compare the memory directly
            return Memory::MemCmp(Data(), other.Data(), ByteSize()) == 0;
        }

        auto it = Begin();
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (!(*it == *otherIt))
            {
                return false;
            }
        }

        return true;
    }

    template <class OtherAllocatorType>
    HYP_FORCE_INLINE bool operator!=(const Array<T, OtherAllocatorType>& other) const
    {
        if (std::addressof(other) == this)
        {
            return false;
        }

        if (Size() != other.Size())
        {
            return true;
        }

        if constexpr (std::is_fundamental_v<T> || std::is_enum_v<T>)
        {
            // if T is a fundamental type, we can compare the memory directly
            return Memory::MemCmp(Data(), other.Data(), ByteSize()) != 0;
        }

        auto it = Begin();
        auto otherIt = other.Begin();
        const auto _end = End();

        for (; it != _end; ++it, ++otherIt)
        {
            if (*it != *otherIt)
            {
                return true;
            }
        }

        return false;
    }

    /*! \brief Creates a Span<T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<T>()
    {
        return Span<T>(Data(), Size());
    }

    /*! \brief Creates a Span<const T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<const T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const T>() const
    {
        return Span<const T>(Data(), Size());
    }

    /*! \brief Creates a Span<T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<T> ToSpan()
    {
        return Span<T>(Data(), Size());
    }

    /*! \brief Creates a Span<const T> from the Array's data.
     *  The span is only valid as long as the Array is not modified.
     *  \return A Span<const T> of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE Span<const T> ToSpan() const
    {
        return Span<const T>(Data(), Size());
    }

    /*! \brief Returns a ByteView of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE ByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull)
    {
        if (offset >= Size())
        {
            return ByteView();
        }

        if (size > Size())
        {
            size = Size();
        }

        return ByteView(reinterpret_cast<ubyte*>(Data()) + offset, size * sizeof(T));
    }

    /*! \brief Returns a ConstByteView of the Array's data. */
    HYP_NODISCARD HYP_FORCE_INLINE ConstByteView ToByteView(SizeType offset = 0, SizeType size = ~0ull) const
    {
        if (offset >= Size())
        {
            return ConstByteView();
        }

        if (size > Size())
        {
            size = Size();
        }

        return ConstByteView(reinterpret_cast<const ubyte*>(Data()) + offset, size * sizeof(T));
    }

    HYP_DEF_STL_BEGIN_END(GetBuffer() + m_startOffset, GetBuffer() + m_size)

protected:
    HYP_FORCE_INLINE T* GetBuffer()
    {
        return m_allocation.GetBuffer();
    }

    HYP_FORCE_INLINE const T* GetBuffer() const
    {
        return m_allocation.GetBuffer();
    }

    void ResetOffsets();

    static SizeType CalculateDesiredCapacity(SizeType size)
    {
        return 1ull << static_cast<SizeType>(std::ceil(std::log(size) / std::log(2.0)));
    }

    SizeType m_size;

protected:
    SizeType m_startOffset;

    Allocation<T, AllocatorType> m_allocation;
};

template <class T, class AllocatorType>
Array<T, AllocatorType>::Array()
    : m_size(0),
      m_startOffset(0)
{
    m_allocation.SetToInitialState();
}

template <class T, class AllocatorType>
Array<T, AllocatorType>::Array(const Array& other)
    : m_size(other.m_size - other.m_startOffset),
      m_startOffset(0)
{
    m_allocation.SetToInitialState();
    m_allocation.Allocate(m_size);
    m_allocation.InitFromRangeCopy(other.Begin(), other.End());
}

template <class T, class AllocatorType>
Array<T, AllocatorType>::Array(Array&& other) noexcept
    : m_size(0),
      m_startOffset(0)
{
    m_allocation.SetToInitialState();

    if (other.m_allocation.IsDynamic())
    {
        m_size = other.m_size;
        m_startOffset = other.m_startOffset;

        m_allocation.TakeOwnership(other.GetBuffer(), other.GetBuffer() + other.m_size);

        other.m_allocation.SetToInitialState();

        other.m_size = 0;
        other.m_startOffset = 0;
    }
    else
    {
        m_size = other.m_size - other.m_startOffset;
        m_startOffset = 0;

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeMove(other.Begin(), other.End());

        other.m_allocation.DestructInRange(other.m_startOffset, other.m_size);
        other.m_allocation.Free();

        other.m_size = 0;
        other.m_startOffset = 0;
    }
}

template <class T, class AllocatorType>
Array<T, AllocatorType>::~Array()
{
    m_allocation.DestructInRange(m_startOffset, m_size);
    m_allocation.Free();
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::operator=(const Array& other) -> Array&
{
    if (this == std::addressof(other))
    {
        return *this;
    }

    m_allocation.DestructInRange(m_startOffset, m_size);
    m_allocation.Free();

    m_size = other.m_size - other.m_startOffset;
    m_startOffset = 0;

    m_allocation.Allocate(m_size);
    m_allocation.InitFromRangeCopy(other.Begin(), other.End());

    return *this;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::operator=(Array&& other) noexcept -> Array&
{
    if (this == std::addressof(other))
    {
        return *this;
    }

    m_allocation.DestructInRange(m_startOffset, m_size);
    m_allocation.Free();

    if (other.m_allocation.IsDynamic())
    {
        m_size = other.m_size;
        m_startOffset = other.m_startOffset;

        m_allocation.TakeOwnership(other.GetBuffer(), other.GetBuffer() + other.m_size);

        other.m_allocation.SetToInitialState();

        other.m_size = 0;
        other.m_startOffset = 0;
    }
    else
    {
        m_size = other.m_size - other.m_startOffset;
        m_startOffset = 0;

        m_allocation.Allocate(m_size);
        m_allocation.InitFromRangeMove(other.Begin(), other.End());

        other.m_allocation.DestructInRange(other.m_startOffset, other.m_size);
        other.m_allocation.Free();

        other.m_size = 0;
        other.m_startOffset = 0;
    }

    return *this;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::ResetOffsets()
{
    if (m_startOffset == 0)
    {
        return;
    }

    T* buffer = GetBuffer();

    // shift all items to left
    for (SizeType index = m_startOffset; index < m_size; ++index)
    {
        const auto moveIndex = index - m_startOffset;

        if constexpr (std::is_move_constructible_v<T>)
        {
            Memory::Construct<T>(&buffer[moveIndex], std::move(buffer[index]));
        }
        else
        {
            Memory::Construct<T>(&buffer[moveIndex], buffer[index]);
        }

        // manual destructor call
        Memory::Destruct(buffer[index]);
    }

    m_size -= m_startOffset;
    m_startOffset = 0;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::SetCapacity(SizeType capacity, SizeType offset)
{
    if (capacity == Capacity() && offset == m_startOffset)
    {
        return;
    }

    HYP_CORE_ASSERT(capacity <= SIZE_MAX / sizeof(T));

    // delete and copy all over again
    Allocation<T, AllocatorType> newAllocation;
    newAllocation.SetToInitialState();
    newAllocation.Allocate(capacity);

    newAllocation.InitFromRangeMove(Begin(), End(), offset);

    m_allocation.DestructInRange(m_startOffset, m_size);
    m_allocation.Free();

    m_size -= m_startOffset;
    m_size += offset;

    m_startOffset = offset;

    m_allocation = newAllocation;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Reserve(SizeType capacity)
{
    if (Capacity() >= capacity)
    {
        return;
    }

    SetCapacity(capacity);
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Resize(SizeType newSize)
{
    const SizeType currentSize = Size();

    if (newSize == currentSize)
    {
        return;
    }

    if (newSize > currentSize)
    {
        const SizeType diff = newSize - currentSize;

        if (m_size + diff >= Capacity())
        {
            if (Capacity() >= currentSize + diff)
            {
                ResetOffsets();
            }
            else
            {
                SetCapacity(CalculateDesiredCapacity(currentSize + diff));
            }
        }

        T* buffer = GetBuffer();

        if constexpr (std::is_fundamental_v<T> || std::is_trivially_constructible_v<T>)
        {
            Memory::MemSet(&buffer[m_size], 0, sizeof(T) * diff);

            m_size += diff;
        }
        else
        {
            while (Size() < newSize)
            {
                // construct item at index
                Memory::Construct<T>(&buffer[m_size++]);
            }
        }
    }
    else
    {
        T* buffer = GetBuffer();

        const SizeType diff = currentSize - newSize;

        for (SizeType i = m_size; i > m_startOffset;)
        {
            Memory::Destruct(buffer[--i]);
        }

        m_size -= diff;
    }
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::ResizeUninitialized(SizeType newSize)
{
    const SizeType currentSize = Size();

    if (newSize == currentSize)
    {
        return;
    }

    if (newSize > currentSize)
    {
        const SizeType diff = newSize - currentSize;

        if (m_size + diff >= Capacity())
        {
            if (Capacity() >= currentSize + diff)
            {
                ResetOffsets();
            }
            else
            {
                SetCapacity(CalculateDesiredCapacity(currentSize + diff));
            }
        }

        m_size += diff;
    }
    else
    {
        T* buffer = GetBuffer();

        const SizeType diff = currentSize - newSize;

        for (SizeType i = m_size; i > m_startOffset;)
        {
            Memory::Destruct(buffer[--i]);
        }

        m_size -= diff;
    }
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::ResizeZeroed(SizeType newSize)
{
    static_assert(std::is_fundamental_v<T> || std::is_trivially_constructible_v<T>,
        "ResizeZeroed can only be used for fundamental or trivially constructible types");

    const SizeType currentSize = Size();

    if (newSize == currentSize)
    {
        return;
    }

    ResizeUninitialized(newSize);

    if (newSize > currentSize)
    {
        Memory::MemSet(GetBuffer() + (currentSize + m_startOffset), 0, sizeof(T) * (newSize - currentSize));
    }
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Refit()
{
    if (Capacity() == Size())
    {
        return;
    }

    SetCapacity(Size());
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PushBack(const ValueType& value) -> ValueType&
{
    if (m_size + 1 >= Capacity())
    {
        if (Capacity() >= Size() + 1)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + 1));
        }
    }

    // set item at index
    T* buffer = GetBuffer();
    T* element = &buffer[m_size++];

    Memory::Construct<T>(element, value);

    return *element;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PushBack(ValueType&& value) -> ValueType&
{
    if (m_size + 1 >= Capacity())
    {
        if (Capacity() >= Size() + 1)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + 1));
        }
    }

    // set item at index
    T* buffer = GetBuffer();
    T* element = &buffer[m_size++];

    Memory::Construct<T>(element, std::move(value));

    return *element;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PushFront(const ValueType& value) -> ValueType&
{
    if (m_startOffset == 0)
    {
        // have to push everything else over by 1
        if (m_size + pushFrontPadding >= Capacity())
        {
            SetCapacity(
                CalculateDesiredCapacity(Size() + pushFrontPadding),
                pushFrontPadding // copyOffset is 1 so we have a space for 1 at the start
            );
        }
        else
        {
            T* buffer = GetBuffer();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;)
            {
                --index;

                const auto moveIndex = index + pushFrontPadding;

                if constexpr (std::is_move_constructible_v<T>)
                {
                    Memory::Construct<T>(&buffer[moveIndex], std::move(buffer[index]));
                }
                else
                {
                    Memory::Construct<T>(&buffer[moveIndex], buffer[index]);
                }

                // manual destructor call
                Memory::Destruct(buffer[index]);
            }

            m_startOffset = pushFrontPadding;
            m_size += m_startOffset;
        }
    }

    // in-place
    --m_startOffset;

    Memory::Construct<T>(&GetBuffer()[m_startOffset], value);

    return Front();
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PushFront(ValueType&& value) -> ValueType&
{
    if (m_startOffset == 0)
    {
        // have to push everything else over by 1
        if (m_size + pushFrontPadding >= Capacity())
        {
            SetCapacity(
                CalculateDesiredCapacity(Size() + pushFrontPadding),
                pushFrontPadding // copyOffset is 1 so we have a space for 1 at the start
            );
        }
        else
        {
            T* buffer = GetBuffer();

            // shift over without realloc
            for (SizeType index = Size(); index > 0;)
            {
                --index;

                const auto moveIndex = index + pushFrontPadding;

                if constexpr (std::is_move_constructible_v<T>)
                {
                    Memory::Construct<T>(&buffer[moveIndex], std::move(buffer[index]));
                }
                else
                {
                    Memory::Construct<T>(&buffer[moveIndex], buffer[index]);
                }

                // manual destructor call
                Memory::Destruct(buffer[index]);
            }

            m_startOffset = pushFrontPadding;
            m_size += m_startOffset;
        }
    }

    // in-place
    --m_startOffset;

    T* buffer = GetBuffer();
    T* element = &buffer[m_startOffset];

    Memory::Construct<T>(element, std::move(value));

    return *element;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Concat(const Array& other)
{
    if (this == &other)
    {
        return;
    }

    if (other.Empty())
    {
        return;
    }

    const SizeType otherSize = other.Size();

    if (m_size + otherSize >= Capacity())
    {
        if (Capacity() >= Size() + otherSize)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + otherSize));
        }
    }

    T* buffer = GetBuffer();

    if constexpr (std::is_fundamental_v<T> || std::is_trivially_copy_constructible_v<T>)
    {
        Memory::MemCpy(&buffer[m_size], other.Data(), otherSize * sizeof(T));

        m_size += otherSize;
    }
    else
    {
        for (SizeType i = 0; i < otherSize; ++i)
        {
            // copy construct item at index
            Memory::Construct<T>(&buffer[m_size++], other.Data()[i]);
        }
    }
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Concat(Array&& other)
{
    if (this == &other)
    {
        return;
    }

    if (other.Empty())
    {
        return;
    }

    const SizeType otherSize = other.Size();

    if (m_size + otherSize >= Capacity())
    {
        if (Capacity() >= Size() + otherSize)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + otherSize));
        }
    }

    T* buffer = GetBuffer();

    if constexpr (std::is_fundamental_v<T> || std::is_trivially_move_constructible_v<T>)
    {
        Memory::MemCpy(&buffer[m_size], other.Data(), otherSize * sizeof(T));

        m_size += otherSize;
    }
    else
    {
        for (SizeType i = 0; i < otherSize; ++i)
        {
            // set item at index
            Memory::Construct<T>(&buffer[m_size++], std::move(other.Data()[i]));
        }
    }

    other.m_allocation.DestructInRange(other.m_startOffset, other.m_size);
    other.m_allocation.Free();

    other.m_size = 0;
    other.m_startOffset = 0;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Shift(SizeType count)
{
    SizeType newSize = 0;

    T* buffer = GetBuffer();

    for (SizeType index = m_startOffset; index < m_size; ++index, ++newSize)
    {
        if (index + count >= m_size)
        {
            break;
        }

        if constexpr (std::is_move_assignable_v<T>)
        {
            buffer[index] = std::move(buffer[index + count]);
        }
        else if constexpr (std::is_move_constructible_v<T>)
        {
            Memory::Destruct(buffer[index]);
            Memory::Construct<T>(&buffer[index], std::move(buffer[index + count]));
        }
        else
        {
            buffer[index] = buffer[index + count];
        }

        // manual destructor call
        Memory::Destruct(buffer[index + count]);
    }

    m_size = newSize;
}

template <class T, class AllocatorType>
Array<T, AllocatorType> Array<T, AllocatorType>::Slice(int first, int last) const
{
    if (first < 0)
    {
        first = Size() + first;
    }

    if (last < 0)
    {
        last = Size() + last;
    }

    if (first < 0)
    {
        first = 0;
    }

    if (last < 0)
    {
        last = 0;
    }

    if (first > last)
    {
        return Array<T, AllocatorType>();
    }

    if (first >= Size())
    {
        return Array<T, AllocatorType>();
    }

    if (last >= Size())
    {
        last = Size() - 1;
    }

    Array<T, AllocatorType> result;
    result.ResizeUninitialized(last - first + 1);

    const T* buffer = GetBuffer();
    T* resultBuffer = result.GetBuffer();

    for (SizeType i = 0; i < result.m_size; ++i)
    {
        Memory::Construct<T>(&resultBuffer[i], buffer[first + i]);
    }

    return result;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Reverse()
{
    if (Size() < 2)
    {
        return;
    }

    T* buffer = GetBuffer();

    SizeType left = m_startOffset;
    SizeType right = m_size - 1;

    while (left < right)
    {
        std::swap(buffer[left], buffer[right]);

        ++left;
        --right;
    }
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::Erase(ConstIterator iter) -> Iterator
{
    const Iterator begin = Begin();
    const Iterator end = End();
    const SizeType sizeOffset = Size();

    if (iter < begin || iter >= end)
    {
        return end;
    }

    const SizeType dist = iter - begin;

    T* buffer = GetBuffer();

    for (SizeType index = dist; index < sizeOffset - 1; ++index)
    {
        if constexpr (std::is_move_constructible_v<T>)
        {
            Memory::Destruct(buffer[m_startOffset + index]);
            Memory::Construct<T>(&buffer[m_startOffset + index], std::move(buffer[m_startOffset + index + 1]));
        }
        else
        {
            Memory::Destruct(buffer[m_startOffset + index]);
            Memory::Construct<T>(&buffer[m_startOffset + index], buffer[m_startOffset + index + 1]);
        }
    }

    Memory::Destruct(buffer[m_size - 1]);
    --m_size;

    return begin + dist;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::Erase(const T& value) -> Iterator
{
    ConstIterator iter = Base::Find(value);

    if (iter != End())
    {
        return Erase(iter);
    }

    return End();
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::EraseAt(typename Array::Base::KeyType index) -> Iterator
{
    return Erase(Begin() + index);
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::Insert(ConstIterator where, const ValueType& value) -> Iterator
{
    const SizeType dist = where - Begin();

    if (where == End())
    {
        PushBack(std::move(value));

        return &GetBuffer()[m_size - 1];
    }
    else if (where == Begin() && dist <= m_startOffset)
    {
        PushFront(std::move(value));

        return Begin();
    }

#ifdef HYP_DEBUG_MODE
    HYP_CORE_ASSERT(where >= Begin() && where <= End());
#endif

    if (m_size + 1 >= Capacity())
    {
        if (Capacity() >= Size() + 1)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + 1));
        }
    }

#ifdef HYP_DEBUG_MODE
    HYP_CORE_ASSERT(Capacity() >= m_size + 1);
#endif

    SizeType index;

    T* buffer = GetBuffer();

    for (index = Size(); index > dist; --index)
    {
        if constexpr (std::is_move_constructible_v<T>)
        {
            Memory::Construct<T>(&buffer[index + m_startOffset], std::move(buffer[index + m_startOffset - 1]));
        }
        else
        {
            Memory::Construct<T>(&buffer[index + m_startOffset], buffer[index + m_startOffset - 1]);
        }

        Memory::Destruct(buffer[index + m_startOffset - 1]);
    }

    Memory::Construct<T>(&buffer[index + m_startOffset], value);

    ++m_size;

    return Begin() + index;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::Insert(ConstIterator where, ValueType&& value) -> Iterator
{
    const SizeType dist = where - Begin();

    if (where == End())
    {
        PushBack(std::move(value));

        return &GetBuffer()[m_size - 1];
    }
    else if (where == Begin() && dist <= m_startOffset)
    {
        PushFront(std::move(value));

        return Begin();
    }

    HYP_CORE_ASSERT(where >= Begin() && where <= End());

    if (m_size + 1 >= Capacity())
    {
        if (Capacity() >= Size() + 1)
        {
            ResetOffsets();
        }
        else
        {
            SetCapacity(CalculateDesiredCapacity(Size() + 1));
        }
    }

    HYP_CORE_ASSERT(Capacity() >= m_size + 1);

    SizeType index;

    T* buffer = GetBuffer();

    for (index = Size(); index > dist; --index)
    {
        if constexpr (std::is_move_constructible_v<T>)
        {
            Memory::Construct<T>(&buffer[index + m_startOffset], std::move(buffer[index + m_startOffset - 1]));
        }
        else
        {
            Memory::Construct<T>(&buffer[index + m_startOffset], buffer[index + m_startOffset - 1]);
        }

        Memory::Destruct(buffer[index + m_startOffset - 1]);
    }

    Memory::Construct<T>(&buffer[index + m_startOffset], std::move(value));

    ++m_size;

    return Begin() + index;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PopFront() -> ValueType
{
    HYP_CORE_ASSERT(Size() != 0);

    auto value = std::move(GetBuffer()[m_startOffset]);

    Memory::Destruct(GetBuffer()[m_startOffset]);

    ++m_startOffset;

    return value;
}

template <class T, class AllocatorType>
auto Array<T, AllocatorType>::PopBack() -> ValueType
{
    HYP_CORE_ASSERT(m_size != 0);

    auto value = std::move(GetBuffer()[m_size - 1]);

    Memory::Destruct(GetBuffer()[m_size - 1]);

    --m_size;

    return value;
}

template <class T, class AllocatorType>
void Array<T, AllocatorType>::Clear()
{
    T* buffer = GetBuffer();

    while (m_size - m_startOffset)
    {
        // manual destructor call
        Memory::Destruct(buffer[m_size - 1]);
        --m_size;
    }

    m_size = 0;
    m_startOffset = 0;

    // Refit();
}

/*! \brief A filter function that applies a predicate to each element in a container.
 *  \param container The container to filter.
 *  \param func The predicate function to apply to each element.
 *  \return A new array containing only the elements that satisfy the predicate. */
template <class ContainerType, class Func>
auto Filter(ContainerType&& container, Func&& func)
{
    using ContainerElementType = typename NormalizedType<ContainerType>::ValueType;
    using PredicateResultType = decltype(std::declval<FunctionWrapper<NormalizedType<Func>>>()(std::declval<ContainerElementType>()));

    FunctionWrapper<NormalizedType<Func>> predicate { std::forward<Func>(func) };

    Array<NormalizedType<ContainerElementType>> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        if (predicate(*it))
        {
            result.PushBack(*it);
        }
    }

    return result;
}

/*! \brief A map function that applies a function to each element in a container.
 *  \param container The container to map over.
 *  \param func The function to apply to each element.
 *  \return A new array with the results of the function applied to each element. */
template <class ContainerType, class Func>
auto Map(ContainerType&& container, Func&& func)
{
    using ContainerElementType = typename NormalizedType<ContainerType>::ValueType;
    using MapResultType = decltype(std::declval<FunctionWrapper<NormalizedType<Func>>>()(std::declval<ContainerElementType>()));

    FunctionWrapper<NormalizedType<Func>> fn { std::forward<Func>(func) };

    Array<NormalizedType<MapResultType>> result;
    result.Reserve(container.Size());

    for (auto it = container.Begin(); it != container.End(); ++it)
    {
        result.PushBack(fn(*it));
    }

    return result;
}

} // namespace containers

using containers::Array;
using containers::Filter;
using containers::Map;

// traits
template <class T>
struct IsArray
{
    enum
    {
        value = false
    };
};

template <class T, class AllocatorType>
struct IsArray<Array<T, AllocatorType>>
{
    enum
    {
        value = true
    };
};

} // namespace hyperion
