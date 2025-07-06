/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <HashCode.hpp>

namespace hyperion {

class LightmapVolume;

HYP_STRUCT(Component)

struct LightmapVolumeComponent
{
    HYP_FIELD(Property = "Volume", Serialize = true)
    Handle<LightmapVolume> volume;
};

} // namespace hyperion
