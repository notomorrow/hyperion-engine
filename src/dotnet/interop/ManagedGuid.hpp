/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_INTEROP_MANAGED_GUID_HPP
#define HYPERION_DOTNET_INTEROP_MANAGED_GUID_HPP

#include <Engine.hpp>
#include <Types.hpp>

#include <type_traits>

namespace hyperion::dotnet {

extern "C" {

struct ManagedGuid
{
    uint64  low;
    uint64  high;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsValid() const
    {
        return low != 0 || high != 0;
    }
};

static_assert(sizeof(ManagedGuid) == 16, "ManagedGuid size mismatch with C#");
static_assert(std::is_standard_layout_v<ManagedGuid>, "ManagedGuid is not standard layout");

} // extern "C"

} // namespace hyperion::dotnet

#endif