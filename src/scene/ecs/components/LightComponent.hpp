/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHT_COMPONENT_HPP
#define HYPERION_ECS_LIGHT_COMPONENT_HPP

#include <core/Handle.hpp>
#include <rendering/Light.hpp>
#include <HashCode.hpp>

namespace hyperion {

using LightComponentFlags = uint32;

enum LightComponentFlagBits : LightComponentFlags
{
    LIGHT_COMPONENT_FLAGS_NONE  = 0x00
};

struct LightComponent
{
    Handle<Light>           light;
    HashCode                transform_hash_code;
    LightComponentFlags     flags = LIGHT_COMPONENT_FLAGS_NONE;
};

static_assert(sizeof(LightComponent) == 24, "LightComponent size mismatch with C#");

} // namespace hyperion

#endif