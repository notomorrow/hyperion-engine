/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SHADOWS_HPP
#define HYPERION_SHADOWS_HPP

#include <core/utilities/EnumFlags.hpp>

#include <Types.hpp>

namespace hyperion {

enum class ShadowMode : uint32
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM,

    MAX
};

enum class ShadowFlags : uint32
{
    NONE                = 0x0,
    PCF                 = 0x1,
    VSM                 = 0x2,
    CONTACT_HARDENED    = 0x4
};

HYP_MAKE_ENUM_FLAGS(ShadowFlags)

} // namespace hyperion

#endif