#ifndef COMPUTE_IRRADIANCE_GLSL
#define COMPUTE_IRRADIANCE_GLSL

#include "Shared.glsl"

ivec3 GetGridCoord(vec3 world_position, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    const ivec3 position_units = ivec3(world_position / size_of_probe + (vec3(grid_size) * 0.5));
    const ivec3 position_offset = position_units % ivec3(grid_size);

    return position_offset;
}

int GridCoordToProbeIndex(ivec3 grid_coord, ivec3 grid_size)
{
    return (int(grid_coord.x) * int(grid_size.y) * int(grid_size.z))
        + (int(grid_coord.y) * int(grid_size.z))
        + int(grid_coord.z) + 1 /* + 1 because the first element is always the reflection probe */;
}

vec3 ComputeLightFieldProbeIrradiance(vec3 world_position, vec3 N, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    const ivec3 base_grid_coord = GetGridCoord(world_position, grid_aabb_extent, grid_size);
    const int base_probe_index_indirect = GridCoordToProbeIndex(base_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;//& (HYP_MAX_BOUND_LIGHT_FIELD_PROBES - 1);
    const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    if (base_probe_index == ~0u) {
        return vec3(0.0);
    }

    const vec3 base_probe_position_world = env_probes[base_probe_index].world_position.xyz;// + size_of_probe * 0.5;

    vec3 alpha = clamp((world_position - base_probe_position_world) / size_of_probe, vec3(0.0), vec3(1.0));

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

    // iterate over probe cage
    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
        ivec3 neighbor_grid_coord = base_grid_coord + offset;

        const int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

        if (neighbor_probe_index == ~0u) {
            continue;
        }

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = trilinear.x * trilinear.y * trilinear.z;

        const vec3 probe_position_world = env_probes[neighbor_probe_index].world_position.xyz;// + size_of_probe * 0.5;
        const vec3 probe_to_point = world_position - probe_position_world;
        const vec3 dir = normalize(-probe_to_point);

        weight *= max(0.05, dot(dir, N));
        
        const vec2 octahedral_dir = EncodeOctahedralCoord(-dir) * 0.5 + 0.5;

        const uvec3 position_in_grid = uvec3(env_probes[neighbor_probe_index].position_in_grid.xyz);

        const uvec2 probe_offset_coord = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (position_in_grid.x * grid_size.y + position_in_grid.y),
            PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
        );

        const uvec2 probe_grid_dimensions = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
            PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
        );

        const vec2 depth_uv = (((vec2(probe_offset_coord) + 0.5) + (octahedral_dir * PROBE_SIDE_LENGTH)) / vec2(probe_grid_dimensions));

        const vec2 moments = Texture2D(sampler_linear, light_field_depth_buffer, depth_uv).rg;
        const float mean = moments.x;
        const float variance = moments.y;

        const float probe_distance = length(probe_to_point);
        const float distance_minus_mean = probe_distance - mean;
        const float chebychev = variance / (variance + distance_minus_mean * distance_minus_mean);

        // TODO: Replace with step()
        weight *= ((distance_minus_mean <= mean) ? 1.0 : max(chebychev, 0.0));
        weight = max(0.0002, weight);

        total_weight += weight;

        const vec3 irradiance_dir = normalize(N);
        const vec2 color_octahedral_uv = EncodeOctahedralCoord(irradiance_dir) * 0.5 + 0.5;
        const vec2 color_uv = (((vec2(probe_offset_coord) + 0.5) + (color_octahedral_uv * PROBE_SIDE_LENGTH)) / vec2(probe_grid_dimensions));

        const vec3 probe_irradiance = Texture2D(sampler_linear, light_field_color_buffer, depth_uv).rgb;//color_uv).rgb;

        total_irradiance += probe_irradiance.rgb * weight;
    }

    total_irradiance = 0.5 * HYP_FMATH_PI * total_irradiance / total_weight;
    // total_irradiance = total_irradiance / total_weight;

    return total_irradiance;
}

#endif