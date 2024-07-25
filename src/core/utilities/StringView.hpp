/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STRING_VIEW_HPP
#define HYPERION_STRING_VIEW_HPP

#include <core/memory/Memory.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <core/containers/StringFwd.hpp>

#include <math/MathUtil.hpp>

#include <HashCode.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

namespace detail {

using namespace containers::detail;

template <int string_type>
class StringView
{
public:
    using CharType = typename containers::detail::StringTypeImpl< string_type >::CharType;
    using WidestCharType = typename containers::detail::StringTypeImpl< string_type >::WidestCharType;

    friend class containers::detail::String<string_type>;

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const StringView<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const StringView<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    static constexpr bool is_contiguous = true;

    static constexpr SizeType not_found = SizeType(-1);

    static constexpr bool is_ansi = string_type == StringType::ANSI;
    static constexpr bool is_utf8 = string_type == StringType::UTF8;
    static constexpr bool is_utf16 = string_type == StringType::UTF16;
    static constexpr bool is_utf32 = string_type == StringType::UTF32;
    static constexpr bool is_wide = string_type == StringType::WIDE_CHAR;

    static_assert(!is_utf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!is_ansi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!is_utf16 || std::is_same_v<CharType, utf::u16char>, "UTF-16 Strings must have CharType equal to utf::u16char");
    static_assert(!is_utf32 || std::is_same_v<CharType, utf::u32char>, "UTF-32 Strings must have CharType equal to utf::u32char");
    static_assert(!is_wide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

private:
    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const CharType *, CharType *>   ptr;

        constexpr IteratorBase(std::conditional_t<IsConst, const CharType *, CharType *> ptr)
            : ptr(ptr)
        {
        }

        HYP_FORCE_INLINE WidestCharType operator*() const
        {
            if constexpr (is_utf8) {
                return utf::char8to32(ptr);
            } else {
                return *ptr;
            }
        }

        HYP_FORCE_INLINE IteratorBase &operator++()
        {
            if constexpr (is_utf8) {
                uint8 num_bytes;
                utf::char8to32(ptr, sizeof(utf::u32char), num_bytes);
                ptr += num_bytes;
            } else {
                ++ptr;
            }

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase operator++(int) const
        {
            if constexpr (is_utf8) {
                uint8 num_bytes;
                utf::char8to32(ptr, sizeof(utf::u32char), num_bytes);

                return { ptr + num_bytes };
            } else {
                return { ptr + 1 };
            }
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase &other) const
            { return ptr == other.ptr; }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase &other) const
            { return ptr != other.ptr; }
    };

public:

    using Iterator = IteratorBase<true>;
    using ConstIterator = Iterator;

private:
    constexpr StringView(const CharType *_begin, const CharType *_end, SizeType length)
        : m_begin(_begin),
          m_end(_end),
          m_length(length)
    {
    }

public:
    constexpr StringView()
        : m_begin(nullptr),
          m_end(nullptr),
          m_length(0)
    {
    }

    // StringView(const detail::String< string_type > &str)
    //     : m_begin(str.Begin()),
    //       m_end(str.End() + 1 /* String class accounts for NUL char also */),
    //       m_length(str.Length())
    // {
    // }

    template <SizeType Sz>
    constexpr StringView(const CharType (&str)[Sz])
        : m_begin(&str[0]),
          m_end(&str[0] + Sz),
          m_length(utf::utf_strlen<CharType, is_utf8>(str))
    {
    }

    constexpr StringView(const CharType *str)
        : m_begin(str),
          m_end(nullptr),
          m_length(0)
    {
        int num_bytes = 0;
        m_length = utf::utf_strlen< CharType, is_utf8 >(str, num_bytes);
        m_end = m_begin + num_bytes;
    }

    constexpr StringView(const CharType *_begin, const CharType *_end)
        : m_begin(_begin),
          m_end(_end),
          m_length(0)
    {
        if constexpr (is_utf8) {
            m_length = utf::utf8_strlen(_begin, _end);
        } else {
            m_length = SizeType(_end - _begin);
        }
    }

    // template <SizeType Sz, std::enable_if_t<std::is_same_v<CharType, typename StaticString<Sz>::CharType>, int> = 0>
    // constexpr StringView(const StaticString<Sz> &str)
    //     : m_begin(str.Begin()),
    //       m_end(str.Begin() + Sz),
    //       m_length(utf::utf_strlen< CharType, is_utf8 >(str.data))
    // {
    // }

    constexpr StringView(const ByteBuffer &byte_buffer)
        : m_begin(reinterpret_cast<const CharType *>(byte_buffer.Data())),
          m_end(reinterpret_cast<const CharType *>(byte_buffer.Data() + byte_buffer.Size())),
          m_length(utf::utf_strlen<CharType, is_utf8>(reinterpret_cast<const CharType *>(byte_buffer.Data())))
    {
    }

    constexpr StringView(ConstByteView byte_view)
        : m_begin(reinterpret_cast<const CharType *>(byte_view.Data())),
          m_end(reinterpret_cast<const CharType *>(byte_view.Data() + byte_view.Size())),
          m_length(utf::utf_strlen<CharType, is_utf8>(reinterpret_cast<const CharType *>(byte_view.Data())))
    {
    }

    constexpr StringView(const StringView &other)   = default;
    StringView &operator=(const StringView &other)  = default;

    constexpr StringView(StringView &&other) noexcept
        : m_begin(other.m_begin),
          m_end(other.m_end),
          m_length(other.m_length)
    {
        other.m_begin = nullptr;
        other.m_end = nullptr;
        other.m_length = 0;
    }

    StringView &operator=(StringView &&other) noexcept
    {
        m_begin = other.m_begin;
        m_end = other.m_end;
        m_length = other.m_length;

        other.m_begin = nullptr;
        other.m_end = nullptr;
        other.m_length = 0;

        return *this;
    }

    constexpr ~StringView() = default;

    // HYP_FORCE_INLINE operator containers::detail::String<string_type>() const
    //     { return containers::detail::String<string_type>(Data()); }

    /*! \brief Check if the StringView is in a valid state.
     *  \returns True if the StringView is valid, false otherwise. */
    HYP_FORCE_INLINE constexpr explicit operator bool() const
        { return uintptr_t(m_end) > uintptr_t(m_begin); }

    /*! \brief Conversion operator to return the charater pointer the StringView is pointing to.
     *  If the StringView is empty, this will return nullptr.
     *  \returns The beginning of the string. */
    HYP_FORCE_INLINE constexpr explicit operator const CharType *() const
        { return m_begin; }

    /*! \brief Convenience operator overload to return the raw string using the dereference operator
     *  \returns The raw string that the StringView has a pointer to. */
    HYP_FORCE_INLINE constexpr const CharType *operator*() const
        { return m_begin; }

    /*! \brief Inversion of the equality operator.
     *  \param other The other StringView object to compare against.
     *  \returns True if the strings are not equal, false otherwise. */
    HYP_FORCE_INLINE constexpr bool operator!=(const StringView &other) const
        { return !(*this == other); }

    /*! \brief Return the size of the string. For UTF-8 strings, this is the number of bytes.
     *  For other types, this is the number of characters.
     *  \note For UTF-8 strings, use the \ref{Length} function to get the number of characters.
     *  \returns The size of the string. */
    HYP_FORCE_INLINE constexpr SizeType Size() const
        { return m_end ? (SizeType(m_end - m_begin)) : 0; }

    /*! \brief Return the length of the string. For UTF-8 strings, this is the number of characters.
     *  For other types, this is the same as the \ref{Size} function.
     *  \returns The length of the string in characters. */
    HYP_FORCE_INLINE constexpr SizeType Length() const
        { return m_length; }

    /*! \brief Return the raw string pointer.
     *  \returns The raw string pointer. If the string is empty, this will return nullptr. */
    HYP_FORCE_INLINE constexpr const CharType *Data() const
        { return m_begin; }

    /*! \brief Get a char from the String at the given index.
     *  For UTF-8 strings, the character is encoded as a 32-bit value.
     *
     *  \ref{index} must be less than \ref{Length()}. */
    WidestCharType GetChar(SizeType index) const
    {
        const SizeType size = Size();

#ifdef HYP_DEBUG_MODE
        AssertThrow(index < size);
#endif

        if constexpr (is_utf8) {
            return utf::utf8_charat(Data(), size, index);
        } else {
            return *(Data() + index);
        }
    }

    constexpr SizeType FindIndex(const StringView &other) const
    {
        const StringView str = StrStr(other);

        if (str.Size() != 0) {
            if constexpr (is_utf8) {
                return utf::utf8_strlen(m_begin, str.m_begin);
            } else {
                return static_cast<SizeType>(str.m_begin - m_begin);
            }
        }

        return not_found;
    }

    constexpr StringView Substr(SizeType first, SizeType last) const
    {
        if (first == SizeType(-1)) {
            return *this;
        }

        first = MathUtil::Min(first, m_length);
        last = MathUtil::Min(MathUtil::Max(last, first), m_length);

        SizeType first_byte_index = 0;
        SizeType last_byte_index = 0;
        SizeType new_length = 0;

        if constexpr (is_utf8) {
            SizeType char_index = 0;

            while (char_index < first) {
                const CharType c = m_begin[first_byte_index];

                if (c >= 0 && c <= 127) {
                    first_byte_index += 1;
                } else if ((c & 0xE0) == 0xC0) {
                    first_byte_index += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    first_byte_index += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    first_byte_index += 4;
                }

                ++char_index;
            }

            while (char_index < last) {
                const CharType c = m_begin[first_byte_index + last_byte_index];

                if (c >= 0 && c <= 127) {
                    last_byte_index += 1;
                } else if ((c & 0xE0) == 0xC0) {
                    last_byte_index += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    last_byte_index += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    last_byte_index += 4;
                }

                ++new_length;
                ++char_index;
            }

            last_byte_index += first_byte_index;
        } else {
            first_byte_index = first;
            last_byte_index = last;
            new_length = last - first;
        }

        return StringView(m_begin + first_byte_index, m_begin + last_byte_index, new_length);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(m_begin, m_end); }

    HYP_DEF_STL_BEGIN_END(
        m_begin,
        m_end
    )

protected:
    constexpr StringView StrStr(const StringView &other) const
    {
        const SizeType this_size = Size();
        const SizeType other_size = other.Size();

        if (this_size < other_size) {
            return StringView();
        }

        for (SizeType offset = 0, other_offset = 0, temp_offset = 0; offset < this_size && m_begin[offset] != '\0'; offset++) {
            if (m_begin[offset] != other.m_begin[other_offset]) {
                continue;
            }

            temp_offset = offset;

            for (;;) {
                if (other_offset >= other_size || other.m_begin[other_offset] == '\0') {
                    return { m_begin + offset, m_end };
                }

                if (temp_offset >= this_size || m_begin[temp_offset] == '\0') {
                    break;
                }

                if (m_begin[temp_offset++] != other.m_begin[other_offset++]) {
                    break;
                }
            }

            other_offset = 0;
        }

        return StringView();
    }

private:
    const CharType  *m_begin;
    const CharType  *m_end;
    SizeType        m_length;
};

template <int string_type>
constexpr bool operator<(const StringView<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (!lhs.Data()) {
        return true;
    }

    if (!rhs.Data()) {
        return false;
    }

    return utf::utf_strcmp<typename StringView<string_type>::CharType, StringView<string_type>::is_utf8>(lhs.Data(), rhs.Data()) < 0;
}

template <int string_type>
constexpr bool operator==(const StringView<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (lhs.Data() == rhs.Data() && (!lhs.Data() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Size(), rhs.Size()));
}

} // namespace detail
} // namespace utilities
} // namespace hyperion

#endif