/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_SHADOWS_HPP
#define HYPERION_SHADOWS_HPP

#include <Types.hpp>

namespace hyperion {

enum class ShadowMode : uint
{
    STANDARD,
    PCF,
    CONTACT_HARDENED,
    VSM,

    MAX
};

} // namespace hyperion

#endif