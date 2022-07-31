#ifndef HYPERION_V2_LIB_STRING_H
#define HYPERION_V2_LIB_STRING_H

#include <util/UTF8.hpp>

#include "DynArray.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {
namespace containers {
namespace detail {
/*! UTF-8 supporting dynamic string class */
using namespace ::utf;

template <class T, bool IsUtf8>
class DynString : DynArray<T> {
protected:
    using Base = typename DynArray<T>::Base;

public:
    using Iterator      = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    static constexpr bool is_utf8 = IsUtf8;

    DynString();
    DynString(const T *str);
    DynString(const DynString &other);
    DynString(DynString &&other) noexcept;
    ~DynString();

    DynString &operator=(const T *str);
    DynString &operator=(const DynString &other);
    DynString &operator=(DynString &&other) noexcept;

    DynString operator+(const DynString &other) const;
    DynString operator+(DynString &&other) const;
    DynString &operator+=(const DynString &other);
    DynString &operator+=(DynString &&other);

    bool operator==(const DynString &other) const;
    bool operator==(const T *str) const;
    bool operator!=(const DynString &other) const;
    bool operator!=(const T *str) const;
    
    [[nodiscard]] const T operator[](SizeType index) const;

    /*! \brief Return the data size in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] typename Base::SizeType Size() const                 { return Base::Size() - 1; /* for NT char */ }
    /*! \brief Return the length of the string in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] typename Base::SizeType Length() const               { return m_length; }

    [[nodiscard]] typename Base::ValueType *Data()                     { return Base::Data(); }
    [[nodiscard]] const typename Base::ValueType *Data() const         { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Front()                    { return Base::Front(); }
    [[nodiscard]] const typename Base::ValueType &Front() const        { return Base::Front(); }

    [[nodiscard]] typename Base::ValueType &Back()                     { return Base::m_values[Base::m_size - 2]; /* for NT char */ }
    [[nodiscard]] const typename Base::ValueType &Back() const         { return Base::m_values[Base::m_size - 2]; /* for NT char */ }

    [[nodiscard]] bool Contains(const T &ch) const                     { return ch != T{0} && Base::Contains(ch); }
    [[nodiscard]] Int FindIndex(const DynString &str) const;

    [[nodiscard]] bool Empty() const                                   { return Size() == 0; }
    [[nodiscard]] bool Any() const                                     { return Size() != 0; }

    [[nodiscard]] bool HasMultiByteChars() const                       { return Size() > Length(); }

    void Reserve(typename Base::SizeType capacity)                     { Base::Reserve(capacity); }
    void Refit()                                                       { Base::Refit(); }
    void Append(const DynString &other);
    void Append(DynString &&other);
    typename Base::ValueType &&PopBack();
    typename Base::ValueType &&PopFront();
    void Clear();

    bool StartsWith(const DynString &other) const;
    bool EndsWith(const DynString &other) const;

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_start_offset]),
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_size])
    )

    friend std::ostream &operator<<(std::ostream &os, const DynString &str)
    {
        os << str.Data();

        return os;
    }
    
#ifdef HYP_UTF8_WIDE
    friend utf::utf8_ostream &operator<<(utf::utf8_ostream &os, const DynString &str)
    {
        os << HYP_UTF8_TOWIDE(str.Data());

        return os;
    }
#endif

protected:
    const T *StrStr(const DynString &other) const;

    typename Base::SizeType m_length;
};

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString()
    : DynArray<T>(),
      m_length(0)
{
    // null-terminated char
    Base::PushBack(T{0});
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const T *str)
    : DynArray<T>()
{
    int count;
    int len = utf::utf_strlen(str, &count);

    if (len == -1) {
        // invalid utf8 string
        return;
    }

    m_length = static_cast<typename Base::SizeType>(len);

    const auto size = static_cast<typename Base::SizeType>(count);
    
    Reserve(size + 1);

    for (SizeType i = 0; i < size; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(T{0});
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const DynString &other)
    : DynArray<T>(other),
      m_length(other.m_length)
{
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(DynString &&other) noexcept
    : DynArray<T>(std::move(other)),
      m_length(other.m_length)
{
    other.m_length = 0;
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::~DynString()
{
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(const T *str) -> DynString&
{
    DynString<T, IsUtf8>::operator=(DynString(str));

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(const DynString &other) -> DynString&
{
    Base::operator=(other);

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(DynString &&other) noexcept -> DynString&
{
    const auto len = other.m_length;
    Base::operator=(std::move(other));
    m_length = len;
    other.m_length = 0;

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+(const DynString &other) const -> DynString
{
    DynString result(*this);
    result.Append(other);

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+(DynString &&other) const -> DynString
{
    DynString result(*this);
    result.Append(std::move(other));

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+=(const DynString &other) -> DynString&
{
    Append(other);

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+=(DynString &&other) -> DynString&
{
    Append(std::move(other));

    return *this;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator==(const DynString &other) const
{
    if (this == std::addressof(other)) {
        return true;
    }

    if (Size() != other.Size() || m_length != other.m_length) {
        return false;
    }

    if (Empty() && other.Empty()) {
        return true;
    }

    return utf::utf_strcmp(&Base::m_values[0], &other.m_values[0]);
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator==(const T *str) const
{
    const auto len = utf::utf_strlen(str);

    if (len == -1) {
        return false; // invalid utf string
    }

    if (m_length != static_cast<typename Base::SizeType>(len)) {
        return false;
    }

    if (Empty() && len == 0) {
        return true;
    }

    return utf::utf_strcmp(&Base::m_values[0], str);
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator!=(const DynString &other) const
{
    return !operator==(other);
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator!=(const T *str) const
{
    return !operator==(str);
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator[](SizeType index) const -> const T
{
    if constexpr (is_utf8) {
        char ch;
        utf::utf8_charat(Data(), &ch, index);
        return static_cast<T>(ch);
    } else {
        return Base::operator[](index);
    }
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(const DynString &other)
{
    if (Base::m_size + other.Size() + 1 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + other.Size() + 1) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + other.Size() + 1));
        }
    }

    Base::PopBack(); // current NT char

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&Base::m_buffer[Base::m_size++].data_buffer) T(other[i]);
    }

    Base::PushBack(T{0});

    m_length += other.m_length;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(DynString &&other)
{
    if (Base::m_size + other.Size() + 1 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + other.Size() + 1) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + other.Size() + 1));
        }
    }

    Base::PopBack(); // current NT char


    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&Base::m_buffer[Base::m_size++].data_buffer) T(std::move(other[i]));
    }

    Base::PushBack(T{0});

    m_length += other.m_length;

    other.Clear();
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::PopFront() -> typename Base::ValueType&&
{
    --m_length;
    return Base::PopFront();
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::PopBack() -> typename Base::ValueType&&
{
    --m_length;
    Base::PopBack(); // pop NT-char
    auto &&res = Base::PopBack();
    Base::PushBack(T{0}); // add NT-char
    return res;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Clear()
{
    Base::Clear();
    Base::PushBack(T{0}); // NT char
    m_length = 0;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::StartsWith(const DynString &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Begin(), Begin() + other.Size(), other.Begin());
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::EndsWith(const DynString &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Begin() + Size() - other.Size(), End(), other.Begin());
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::StrStr(const DynString &other) const -> const T*
{
    if (Size() < other.Size()) {
        return nullptr;
    }

    const T *a, *b;
    b = other.Data();

    for (const T *str = Data(); *str != 0; ++str) {
        if (*str != *b) {
            continue;
        }

        a = str;

        for (;;) {
            if (*b == 0) {
                return str;
            }

            if (*a++ != *b++) {
                break;
            }
        }

        b = other.Data();
    }

    return nullptr;
}

template <class T, bool IsUtf8>
Int DynString<T, IsUtf8>::FindIndex(const DynString &other) const
{
    if (auto *ptr = StrStr(other)) {
        return static_cast<Int>(ptr - Data());
    }

    return -1;
}

} // namespace detail
} // namespace containers

using String = containers::detail::DynString<char, true>;
using ANSIString = containers::detail::DynString<char, false>;
using WideString = containers::detail::DynString<wchar_t, false>;

} // namespace hyperion

#endif