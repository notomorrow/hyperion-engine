/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/StaticString.hpp>
#include <core/Defines.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

class NameRegistry;

using NameID = uint64;

struct Name;
struct WeakName;

template <auto StaticStringType>
struct HashedName;

template <HashCode::ValueType HashCode>
static inline Name CreateNameFromStaticString_WithLock(const char* str);

extern HYP_API Name CreateNameFromDynamicString(const ANSIString& str);

/*! \brief A name is a hashed string that is used to identify objects, components, and other entities in the engine.
 *  \details Names have their text components stored in a global registry and are internally.
 *  A \ref{Name} holds a 64 bit unsigned integer representing the hash, allowing for fast lookups and comparisons.
 *
 *  To create a name at compile time, use the \ref{HYP_NAME} macro.
 *  \code{.cpp}
 *  Name name = NAME("MyName");
 *  \endcode
 *
 *  To create a name at runtime, use the \ref{CreateNameFromDynamicString} function.
 *  \code{.cpp}
 *  Name name = CreateNameFromDynamicString("MyName");
 *  \endcode
 */
struct Name
{
    friend constexpr bool operator==(const Name& lhs, const Name& rhs);
    friend constexpr bool operator==(const Name& lhs, const WeakName& rhs);
    friend constexpr bool operator!=(const Name& lhs, const Name& rhs);
    friend constexpr bool operator!=(const Name& lhs, const WeakName& rhs);
    friend constexpr bool operator<(const Name& lhs, const Name& rhs);
    friend constexpr bool operator<(const Name& lhs, const WeakName& rhs);
    friend constexpr bool operator<=(const Name& lhs, const Name& rhs);
    friend constexpr bool operator<=(const Name& lhs, const WeakName& rhs);
    friend constexpr bool operator>(const Name& lhs, const Name& rhs);
    friend constexpr bool operator>(const Name& lhs, const WeakName& rhs);
    friend constexpr bool operator>=(const Name& lhs, const Name& rhs);
    friend constexpr bool operator>=(const Name& lhs, const WeakName& rhs);

    static NameRegistry* s_registry;

    HashCode::ValueType hashCode;

    constexpr Name()
        : hashCode(0)
    {
    }

    constexpr explicit Name(NameID id)
        : hashCode(id)
    {
    }

    constexpr Name(const Name& other) = default;
    constexpr Name& operator=(const Name& other) = default;
    constexpr Name(Name&& other) noexcept = default;
    constexpr Name& operator=(Name&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr NameID Id() const
    {
        return hashCode;
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return hashCode != 0;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr explicit operator uint64() const
    {
        return hashCode;
    }

    /*! \brief For convenience, operator* is overloaded to return the string representation of the name,
     *  if it is found in the name registry. Otherwise, it returns an empty string. */
    HYP_FORCE_INLINE const char* operator*() const
    {
        return LookupString();
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(hashCode));
    }

    /*! \brief Returns the string representation of the name, if it is found in the name registry.
     *  Otherwise, it returns an empty string. */
    HYP_API const char* LookupString() const;

    HYP_API static NameRegistry* GetRegistry();

    /*! \brief Generates a unique name with a prefix. */
    HYP_API static Name Unique(ANSIStringView prefix);

    HYP_FORCE_INLINE static constexpr Name Invalid()
    {
        return Name { 0 };
    };

    // template <class StaticStringType>
    // HYP_NODISCARD
    // HYP_FORCE_INLINE
    // static constexpr Name FromStaticString(StaticStringType staticString)
    //     { return Name { (CreateNameFromStaticString_WithLock<HashCode::GetHashCode(StaticString)>(StaticString.data).hashCode) }; }
};

struct WeakName
{
    friend constexpr bool operator==(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator==(const WeakName& lhs, const Name& rhs);
    friend constexpr bool operator!=(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator!=(const WeakName& lhs, const Name& rhs);
    friend constexpr bool operator<(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator<(const WeakName& lhs, const Name& rhs);
    friend constexpr bool operator<=(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator<=(const WeakName& lhs, const Name& rhs);
    friend constexpr bool operator>(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator>(const WeakName& lhs, const Name& rhs);
    friend constexpr bool operator>=(const WeakName& lhs, const WeakName& rhs);
    friend constexpr bool operator>=(const WeakName& lhs, const Name& rhs);

    HashCode::ValueType hashCode;

    constexpr WeakName()
        : hashCode(0)
    {
    }

    constexpr explicit WeakName(NameID id)
        : hashCode(id)
    {
    }

    template <SizeType Sz>
    constexpr WeakName(const char (&str)[Sz])
        : hashCode(HashCode::GetHashCode(str).Value())
    {
    }

    constexpr WeakName(const char* str)
        : hashCode(HashCode::GetHashCode(str).Value())
    {
    }

    template <int StringType, typename = std::enable_if_t<std::is_same_v<typename StringView<StringType>::CharType, char>>>
    constexpr WeakName(const StringView<StringType>& str)
        : hashCode(HashCode::GetHashCode(str).Value())
    {
    }

    template <int StringType, typename = std::enable_if_t<std::is_same_v<typename containers::String<StringType>::CharType, char>>>
    constexpr WeakName(const containers::String<StringType>& str)
        : hashCode(HashCode::GetHashCode(str).Value())
    {
    }

    constexpr WeakName(const Name& name)
        : hashCode(name.hashCode)
    {
    }

    constexpr WeakName& operator=(const Name& name)
    {
        hashCode = name.hashCode;

        return *this;
    }

    constexpr WeakName(const WeakName& other) = default;
    constexpr WeakName& operator=(const WeakName& other) = default;
    constexpr WeakName(WeakName&& other) noexcept = default;
    constexpr WeakName& operator=(WeakName&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr NameID Id() const
    {
        return hashCode;
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return hashCode != 0;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr explicit operator uint64() const
    {
        return hashCode;
    }

    HYP_FORCE_INLINE constexpr explicit operator Name() const
    {
        Name name;
        name.hashCode = hashCode;

        return name;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(hashCode));
    }

    HYP_FORCE_INLINE static constexpr WeakName Invalid()
    {
        return WeakName {};
    };
};

constexpr bool operator==(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode == rhs.hashCode;
}

constexpr bool operator==(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode == rhs.hashCode;
}

constexpr bool operator!=(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode != rhs.hashCode;
}

constexpr bool operator!=(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode != rhs.hashCode;
}

constexpr bool operator<(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode < rhs.hashCode;
}

constexpr bool operator<(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode < rhs.hashCode;
}

constexpr bool operator<=(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode <= rhs.hashCode;
}

constexpr bool operator<=(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode <= rhs.hashCode;
}

constexpr bool operator>(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode > rhs.hashCode;
}

constexpr bool operator>(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode > rhs.hashCode;
}

constexpr bool operator>=(const Name& lhs, const Name& rhs)
{
    return lhs.hashCode >= rhs.hashCode;
}

constexpr bool operator>=(const Name& lhs, const WeakName& rhs)
{
    return lhs.hashCode >= rhs.hashCode;
}

constexpr bool operator==(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode == rhs.hashCode;
}

constexpr bool operator==(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode == rhs.hashCode;
}

constexpr bool operator!=(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode != rhs.hashCode;
}

constexpr bool operator!=(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode != rhs.hashCode;
}

constexpr bool operator<(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode < rhs.hashCode;
}

constexpr bool operator<(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode < rhs.hashCode;
}

constexpr bool operator<=(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode <= rhs.hashCode;
}

constexpr bool operator<=(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode <= rhs.hashCode;
}

constexpr bool operator>(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode > rhs.hashCode;
}

constexpr bool operator>(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode > rhs.hashCode;
}

constexpr bool operator>=(const WeakName& lhs, const WeakName& rhs)
{
    return lhs.hashCode >= rhs.hashCode;
}

constexpr bool operator>=(const WeakName& lhs, const Name& rhs)
{
    return lhs.hashCode >= rhs.hashCode;
}

template <auto StaticStringType>
struct HashedName
{
    using Sequence = containers::IntegerSequenceFromString<StaticStringType>;

    static constexpr HashCode hashCode = HashCode::GetHashCode(Sequence::Data());
    static constexpr const char* data = Sequence::Data();
};

} // namespace hyperion
