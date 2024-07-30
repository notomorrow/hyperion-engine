/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TYPE_ID_HPP
#define HYPERION_TYPE_ID_HPP

#include <core/threading/Mutex.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {
namespace utilities {

using TypeIDValue = uint32;

namespace detail {

template <class T>
struct TypeID_Impl
{
    static constexpr TypeIDValue value = TypeNameWithoutNamespace<T>().GetHashCode().Value() % HashCode::ValueType(MathUtil::MaxSafeValue<TypeIDValue>());
};

template <>
struct TypeID_Impl<void>
{
    static constexpr TypeIDValue value = 0;
};

} // namespace detail

/*! \brief Simple 32-bit identifier for a given type. Stable across DLLs as the type hash is based on the name of the type. */
struct TypeID
{
    using ValueType = TypeIDValue;

private:
    ValueType   value;

    static constexpr ValueType void_value = ValueType(0);

public:
    template <class T>
    static constexpr TypeID ForType()
        { return TypeID { detail::TypeID_Impl<T>::value }; }

    template <SizeType Sz>
    static constexpr TypeID FromString(const char (&str)[Sz])
    {
        return TypeID {
            ValueType(HashCode::GetHashCode(str).Value() % HashCode::ValueType(MathUtil::MaxSafeValue<ValueType>()))
        };
    }

    static TypeID FromString(const char *str)
    {
        return TypeID {
            ValueType(HashCode::GetHashCode(str).Value() % HashCode::ValueType(MathUtil::MaxSafeValue<ValueType>()))
        };
    }

    constexpr TypeID()
        : value { void_value }
    {
    }

    constexpr TypeID(ValueType id)
        : value(id)
    {
    }

    constexpr TypeID(const TypeID &other)   = default;
    TypeID &operator=(const TypeID &other)  = default;

    constexpr TypeID(TypeID &&other) noexcept
        : value(other.value)
    {
        other.value = void_value;
    }
    
    constexpr TypeID &operator=(TypeID &&other) noexcept
    {
        value = other.value;
        other.value = void_value;

        return *this;
    }

    constexpr TypeID &operator=(ValueType id)
    {
        value = id;

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr explicit operator bool() const
        { return value != void_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!() const
        { return value == void_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator==(const TypeID &other) const
        { return value == other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!=(const TypeID &other) const
        { return value != other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator<(const TypeID &other) const
        { return value < other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator<=(const TypeID &other) const
        { return value <= other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator>(const TypeID &other) const
        { return value > other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator>=(const TypeID &other) const
        { return value >= other.value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr ValueType Value() const
        { return value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    static constexpr TypeID Void()
        { return TypeID { void_value }; }
};

} // namespace utilities

using utilities::TypeID;

} // namespace hyperion

#endif
