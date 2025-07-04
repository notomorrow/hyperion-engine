/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_ID_HPP
#define HYPERION_CORE_ID_HPP

#include <core/utilities/TypeID.hpp>
#include <core/utilities/UniqueID.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

namespace hyperion {

struct IDBase
{
    constexpr IDBase() = default;

    constexpr IDBase(TypeID type_id, uint32 value)
        : type_id_value(type_id.Value()),
          value(value)
    {
    }

    IDBase(uint32 value) = delete; // delete so we cannot create an IDBase with just a value, it must have a type_id

    constexpr IDBase(const IDBase& other) = default;
    IDBase& operator=(const IDBase& other) = default;

    constexpr IDBase(IDBase&& other) noexcept = default;
    IDBase& operator=(IDBase&& other) noexcept = default;

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return type_id_value != 0 && value != 0;
    }

    HYP_FORCE_INLINE constexpr uint32 Value() const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr TypeID GetTypeID() const
    {
        return TypeID { type_id_value };
    }

    HYP_FORCE_INLINE explicit constexpr operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return !IsValid();
    }

    HYP_FORCE_INLINE constexpr bool operator==(const IDBase& other) const
    {
        return type_id_value == other.type_id_value && value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const IDBase& other) const
    {
        return type_id_value != other.type_id_value || value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const IDBase& other) const
    {
        if (type_id_value != other.type_id_value)
        {
            return type_id_value < other.type_id_value;
        }

        return value < other.value;
    }

    /*! \brief If the value is non-zero, returns the ID minus one,
        to be used as a storage index. If the value is zero (invalid state),
        zero is returned. Ideally a validation check would be performed before you use this,
        unless you are totally sure that 0 is a valid index. */
    HYP_FORCE_INLINE constexpr uint32 ToIndex(uint32 invalid_value = 0) const
    {
        return value ? value - 1 : invalid_value;
    }

    HYP_FORCE_INLINE operator UniqueID() const
    {
        return UniqueID { GetHashCode() };
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type_id_value);
        hc.Add(value);

        return hc;
    }

    uint32 type_id_value = 0;
    uint32 value = 0;
};

/*! \brief A transient, global ID for an instance of an object. Object is not guaranteed to be alive when this ID is used.
 *  \details The object this is referencing may not be of type T as it may be a subclass of T. Use TypeID() to get the runtime type ID of the object.
 *  \warning This ID is NOT guaranteed to be stable across runs of the engine. Do not use it for persistent storage or serialization. */
template <class T>
struct ID : IDBase
{
    static const ID invalid;

    constexpr ID()
        : IDBase { TypeID::ForType<T>(), 0 }
    {
    }

    explicit constexpr ID(const IDBase& other)
        : IDBase(other)
    {
    }

    ID& operator=(const IDBase& other) = delete; // delete so we cannot assign a type's ID to a different type

    constexpr ID(const ID& other) = default;
    ID& operator=(const ID& other) = default;

    constexpr ID(ID&& other) noexcept = default;
    ID& operator=(ID&& other) noexcept = default;

    static ID FromIndex(uint32 index)
    {
        return ID { IDBase { TypeID::ForType<T>(), index + 1 } };
    }

    /*! \brief Allows implicit conversion to ID<Ty> where T is convertible to Ty.
     *  \details This is useful for converting IDs of derived types to IDs of base types. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, int> = 0>
    HYP_FORCE_INLINE operator ID<Ty>() const
    {
        return ID<Ty>(static_cast<const IDBase&>(*this));
    }

    /*! \brief Allows explicit conversion to ID<Ty> where T is a base class of Ty.
     *  \details This is useful for converting IDs of base types to IDs of derived types, but requires an explicit cast to avoid accidental conversions. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && (!std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> && std::is_base_of_v<T, Ty>), int> = 0>
    HYP_FORCE_INLINE explicit operator ID<Ty>() const
    {
        if (!IsValid())
        {
            return ID<Ty>::invalid;
        }

        /// \todo Add a AssertDebug here to ensure that type_id is in the hierarchy of Ty

        return ID<Ty>(static_cast<const IDBase&>(*this));
    }
};

template <class T>
const ID<T> ID<T>::invalid = ID<T>();

} // namespace hyperion

#endif