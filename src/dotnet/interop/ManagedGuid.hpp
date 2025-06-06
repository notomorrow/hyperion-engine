/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_INTEROP_MANAGED_GUID_HPP
#define HYPERION_DOTNET_INTEROP_MANAGED_GUID_HPP

#include <core/utilities/UUID.hpp>

#include <core/utilities/FormatFwd.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion::dotnet {

extern "C"
{

    struct ManagedGuid
    {
        uint64 low;
        uint64 high;

        bool operator==(const ManagedGuid& other) const = default;
        bool operator!=(const ManagedGuid& other) const = default;

        HYP_FORCE_INLINE bool IsValid() const
        {
            return low != 0 || high != 0;
        }

        HYP_FORCE_INLINE UUID ToUUID() const
        {
            return UUID(low, high);
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            return HashCode(low).Combine(high);
        }
    };

    static_assert(sizeof(ManagedGuid) == 16, "ManagedGuid size mismatch with C#");
    static_assert(std::is_standard_layout_v<ManagedGuid>, "ManagedGuid is not standard layout");

} // extern "C"

} // namespace hyperion::dotnet

namespace hyperion {

// formatter
namespace utilities {

template <class StringType>
struct Formatter<StringType, dotnet::ManagedGuid>
{
    constexpr auto operator()(const dotnet::ManagedGuid& value) const
    {
        return StringType(value.ToUUID().ToString());
    }
};

} // namespace utilities

} // namespace hyperion

#endif