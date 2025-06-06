/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_TERRAIN_COMPONENT_HPP
#define HYPERION_ECS_TERRAIN_COMPONENT_HPP

#include <core/math/Vector3.hpp>
#include <core/math/Extent.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {

using TerrainPatchCoord = Vec2i;

enum class TerrainPatchState
{
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

HYP_STRUCT()

struct TerrainPatchNeighbor
{
    HYP_FIELD(Serialize, Property = "Coord")
    TerrainPatchCoord coord;

    HYP_FORCE_INLINE Vec2f GetCenter() const
    {
        return Vec2f(coord) - 0.5f;
    }
};

HYP_STRUCT()

struct TerrainPatchInfo
{
    HYP_FIELD(Serialize, Property = "Extent")
    Vec3u extent;

    HYP_FIELD(Serialize, Property = "Coord")
    TerrainPatchCoord coord;

    HYP_FIELD(Serialize, Property = "Scale")
    Vec3f scale = Vec3f::One();

    HYP_FIELD(Serialize, Property = "State")
    TerrainPatchState state = TerrainPatchState::UNLOADED;

    HYP_FIELD(Serialize, Property = "Neighbors")
    FixedArray<TerrainPatchNeighbor, 8> neighbors;

    HYP_FIELD()
    float unload_timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;
        hash_code.Add(extent);
        hash_code.Add(coord);
        hash_code.Add(scale);
        hash_code.Add(state);

        return hash_code;
    }
};

using TerrainComponentFlags = uint32;

enum TerrainComponentFlagBits : TerrainComponentFlags
{
    TERRAIN_COMPONENT_FLAG_NONE = 0x0,
    TERRAIN_COMPONENT_FLAG_INIT = 0x1
};

HYP_STRUCT(Component, Label = "Terrain Component", Description = "Controls dynamic terrain.", Editor = true)

struct TerrainComponent
{
    HYP_FIELD(Property = "Seed", Serialize = true, Editor = true)
    uint32 seed = 0;

    HYP_FIELD(Property = "PatchSize", Serialize = true, Editor = true)
    Vec3u patch_size = { 32, 32, 32 };

    HYP_FIELD(Property = "Scale", Serialize = true, Editor = true)
    Vec3f scale = Vec3f::One();

    HYP_FIELD(Property = "MaxDistance", Serialize = true, Editor = true)
    float max_distance = 2.0f;

    HYP_FIELD()
    TerrainComponentFlags flags = TERRAIN_COMPONENT_FLAG_NONE;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(seed);
        hash_code.Add(patch_size);
        hash_code.Add(scale);
        hash_code.Add(max_distance);

        return hash_code;
    }
};

} // namespace hyperion

#endif