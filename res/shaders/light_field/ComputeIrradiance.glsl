#ifndef COMPUTE_IRRADIANCE_GLSL
#define COMPUTE_IRRADIANCE_GLSL

#include "Shared.glsl"

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

int GridCoordToProbeIndex(ivec3 grid_coord, ivec3 grid_size)
{
    return (int(grid_coord.x) * int(grid_size.y) * int(grid_size.z))
        + (int(grid_coord.y) * int(grid_size.z))
        + int(grid_coord.z) + 1 /* + 1 because the first element is always the reflection probe */;
}

vec2 TextureCoordFromDirection(vec3 dir, uint probe_index, uvec2 image_dimensions, ivec3 grid_size)
{
    vec2 normalizedOctCoord = EncodeOctahedralCoord(normalize(dir));
    vec2 normalizedOctCoordZeroOne = (normalizedOctCoord + vec2(1.0f)) * 0.5f;

    vec2 octCoordNormalizedToTextureDimensions = (normalizedOctCoordZeroOne * float(PROBE_SIDE_LENGTH)) / vec2(image_dimensions);

    uint probesPerRow = (image_dimensions.x - 2) / uint(PROBE_SIDE_LENGTH_BORDER);

    // border around texture and further 1 pix border around top left probe.
    vec2 probeTopLeftPosition = vec2(mod(probe_index, probesPerRow) * PROBE_SIDE_LENGTH_BORDER,
        (probe_index / probesPerRow) * PROBE_SIDE_LENGTH_BORDER) + vec2(2.0f, 2.0f);

    vec2 normalizedProbeTopLeftPosition = vec2(probeTopLeftPosition) / vec2(image_dimensions);

    //return vec2(normalizedProbeTopLeftPosition + octCoordNormalizedToTextureDimensions);

    const uvec3 position_in_grid = uvec3(env_probes[probe_index].position_in_grid.xyz);

    const uvec2 probe_offset_coord = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (position_in_grid.x * grid_size.y + position_in_grid.y),
        PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
    ) + uvec2(2);


    return vec2(((vec2(probe_offset_coord) + 0.5) / vec2(image_dimensions)) + octCoordNormalizedToTextureDimensions);
}



vec3 ComputeLightFieldProbeIrradiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    const ivec3 base_grid_coord = GetGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);
    const int base_probe_index_indirect = GridCoordToProbeIndex(base_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;//& (HYP_MAX_BOUND_LIGHT_FIELD_PROBES - 1);
    const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    if (base_probe_index == ~0u) {
        return vec3(0.0);
    }

    const vec3 base_probe_position_world = env_probes[base_probe_index].world_position.xyz;

    vec3 alpha = clamp((world_position - base_probe_position_world) / size_of_probe, vec3(0.0), vec3(1.0));

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

    // iterate over probe cage
    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
        ivec3 neighbor_grid_coord = clamp(base_grid_coord + offset, ivec3(0), ivec3(grid_size) - 1);

        const int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

        if (neighbor_probe_index == ~0u) {
            continue;
        }

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = trilinear.x * trilinear.y * trilinear.z;

        const vec3 probe_position_world = env_probes[neighbor_probe_index].world_position.xyz;
        const vec3 probe_to_point = world_position - probe_position_world;
        const vec3 dir = normalize(-probe_to_point);

        vec3 true_direction_to_probe = normalize(probe_position_world - world_position);
        weight *= max(0.08, dot(dir, N));
        //weight *= HYP_FMATH_SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        const vec2 octahedral_dir = EncodeOctahedralCoord(-dir) * 0.5 + 0.5;

        const uvec3 position_in_grid = uvec3(env_probes[neighbor_probe_index].position_in_grid.xyz);

        const uvec2 probe_offset_coord = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (position_in_grid.y * grid_size.x + position_in_grid.x),
            PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
        ) + 2;

        const uvec2 probe_grid_dimensions = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
            PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
        );
        
        /*const vec3 probe_aabb_min = (vec3(position_in_grid) * grid_aabb_extent / vec3(grid_size)));
        const vec3 probe_aabb_max = (vec3(position_in_grid + 1) * (grid_aabb_extent / vec3(grid_size)));
        const vec3 extent = (probe_aabb_max - probe_aabb_min);
        const vec3 extent_unpadded = grid_aabb_extent / vec3(grid_size);
        const vec3 center = (probe_aabb_max + probe_aabb_min) * 0.5;
        const vec3 pos_fract = fract(((world_position - center) / extent) + 0.5);
        const vec3 texel_size = vec3(1.0) / vec3(probe_grid_dimensions);
        vec3 coord = (vec3(vec3(probe_offset_coord) + pos_fract)) * texel_size;*/

        const vec2 depth_uv = (vec2(probe_offset_coord) + (octahedral_dir * PROBE_SIDE_LENGTH) + 0.5) / vec2(probe_grid_dimensions);

        //const vec2 depth_uv = TextureCoordFromDirection(-dir, neighbor_probe_index, probe_grid_dimensions, grid_size);

        const vec2 moments = Texture2DLod(sampler_linear, light_field_depth_buffer, depth_uv, 0.0).rg;
        const float mean = moments.x;
        const float variance = abs(moments.y - HYP_FMATH_SQR(mean));

        const float probe_distance = length(probe_to_point);
        const float distance_minus_mean = probe_distance - mean;

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(probe_distance - mean, 0.0)));
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (probe_distance <= mean) ? 1.0 : chebyshev;
        weight = max(0.0001, weight);

        const vec3 irradiance_dir = N;

        const vec2 color_octahedral_uv = EncodeOctahedralCoord(irradiance_dir) * 0.5 + 0.5;
        const vec2 irradiance_uv = (vec2(probe_offset_coord) + (color_octahedral_uv * PROBE_SIDE_LENGTH) + 0.5) / vec2(probe_grid_dimensions);

        //vec2 irradiance_uv = TextureCoordFromDirection(irradiance_dir, neighbor_probe_index, probe_grid_dimensions, grid_size);
        vec3 irradiance = Texture2DLod(sampler_linear, light_field_color_buffer, depth_uv, 0.0).rgb;

            
        const float crush_threshold = 0.2;
        if (weight < crush_threshold) {
            weight *= weight * weight * (1.0 / HYP_FMATH_SQR(crush_threshold));
        }

        irradiance = sqrt(irradiance);

        //weight = 1.0 / 8.0;
        
        total_irradiance += irradiance * weight;
        //total_irradiance += vec3(neighbor_grid_coord) * 0.2 * weight;
        total_weight += weight;
    }

    //total_irradiance = 0.5 * HYP_FMATH_PI * total_irradiance / total_weight;
    vec3 net_irradiance = total_irradiance / total_weight;
    net_irradiance = HYP_FMATH_SQR(net_irradiance);

    return net_irradiance;
}

#endif