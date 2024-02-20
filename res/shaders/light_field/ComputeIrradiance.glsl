#ifndef COMPUTE_IRRADIANCE_GLSL
#define COMPUTE_IRRADIANCE_GLSL

#include "Shared.glsl"
#include "../include/noise.inc"
#include "../include/packing.inc"
#include "../include/shared.inc"

#define HYP_LIGHT_FIELD_PROBE_MAX_RADIANCE_SAMPLES 8

#define TraceResult int
#define TRACE_RESULT_MISS 0
#define TRACE_RESULT_HIT 1
#define TRACE_RESULT_UNKNOWN 2

const float minThickness = 0.03; // meters
const float maxThickness = 0.50; // meters
const float rayBumpEpsilon = 0.01; // meters

const vec2 TEX_SIZE = vec2(PROBE_SIDE_LENGTH);
const vec2 TEX_SIZE_SMALL = vec2(PROBE_SIDE_LENGTH_LOWRES); // temp until we have a small depth buffer

const vec2 INV_TEX_SIZE = vec2(1.0) / TEX_SIZE;
const vec2 INV_TEX_SIZE_SMALL = vec2(1.0) / TEX_SIZE_SMALL;

#define NEXT_CYCLE_INDEX(idx) ((idx + 3) & 7)

ivec3 GetGridCoord(vec3 world_position, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 diff = world_position - grid_center;
    const vec3 pos_clamped = (diff / env_grid.aabb_extent.xyz) + 0.5;
    const ivec3 unit_diff = ivec3(pos_clamped * vec3(grid_size));
    return unit_diff;

    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    const ivec3 position_units = ivec3(world_position / size_of_probe + (vec3(grid_size) * 0.5));
    const ivec3 position_offset = position_units % ivec3(grid_size);

    return position_offset;
}

vec3 GridCoordToWorldPosition(ivec3 grid_coord, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    return size_of_probe * vec3(grid_coord) + (grid_center - (grid_aabb_extent * 0.5));
}

int GridCoordToProbeIndex(ivec3 grid_coord, ivec3 grid_size)
{
    return (int(grid_coord.x) * int(grid_size.y) * int(grid_size.z))
        + (int(grid_coord.y) * int(grid_size.z))
        + int(grid_coord.z);
}

ivec3 BaseGridCoord(vec3 P, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    vec3 grid_start_position = grid_center - (grid_aabb_extent * 0.5);
    vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    
    return clamp(ivec3((P - grid_start_position) / size_of_probe),
        ivec3(0),
        grid_size - ivec3(1));
}

vec2 LocalTexCoordToProbeTexCoord(vec2 uv, uint probe_index, uvec2 image_dimensions, ivec3 grid_size)
{

    const uvec3 position_in_grid = uvec3(env_probes[probe_index].position_in_grid.xyz);

    const uvec2 probe_offset_coord = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (position_in_grid.y * grid_size.x + position_in_grid.x),
        PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
    ) + PROBE_BORDER_LENGTH;

    const uvec2 probe_grid_dimensions = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
    );

    uvec2 coord = uvec2(uv * float(PROBE_SIDE_LENGTH)) - 1;

    return ((vec2(probe_offset_coord) + vec2(coord) + 1) + 0.5) / vec2(image_dimensions);

    // vec2 normalized_coord_to_texture_dimensions = (uv * float(PROBE_SIDE_LENGTH)) / vec2(image_dimensions);

    // uint probes_per_row = (image_dimensions.x - 2) / uint(PROBE_SIDE_LENGTH_BORDER);

    // // border around texture and further 1 pix border around top left probe.
    // vec2 probe_top_left_position = vec2(mod(probe_index, probes_per_row) * PROBE_SIDE_LENGTH_BORDER,
    //     (probe_index / probes_per_row) * PROBE_SIDE_LENGTH_BORDER) + vec2(2.0f, 2.0f);

    // vec2 normalized_probe_top_left_position = vec2(probe_top_left_position) / vec2(image_dimensions);

    // return vec2(normalized_probe_top_left_position + normalized_coord_to_texture_dimensions);
}

#endif