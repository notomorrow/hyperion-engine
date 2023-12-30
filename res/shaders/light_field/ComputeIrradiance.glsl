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

uvec2 GetProbeOffsetCoord(uint probe_index, uvec2 image_dimensions, ivec3 grid_size)
{
    // Get probe index in texture
    ivec3 position_in_grid = ivec3(env_probes[probe_index].position_in_grid.xyz);

    return uvec2(
        PROBE_SIDE_LENGTH_BORDER * (position_in_grid.x * grid_size.y + position_in_grid.y),
        PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
    ) + uvec2(PROBE_BORDER_LENGTH);
}

vec3 ReadProbeColor(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = env_probes[probe_index].position_in_grid.w;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_color_buffer, sampler_linear), vec3(uv, float(probe_index)), 0.0).rgb;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_color_buffer, sampler_linear), probe_uv, 0.0).rgb;
#endif
}

vec3 ReadProbeNormals(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = env_probes[probe_index].position_in_grid.w;

#ifdef USE_TEXTURE_ARRAY
    return UnpackNormalVec2(texture(sampler2DArray(light_field_normals_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).rg);
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return UnpackNormalVec2(texture(sampler2D(light_field_normals_buffer, sampler_nearest), probe_uv, 0.0).rg);
#endif
}

float ReadProbeDepth(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = env_probes[probe_index].position_in_grid.w;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).r;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_depth_buffer, sampler_nearest), probe_uv, 0.0).r;
#endif
}

vec3 ReadProbeColorLowRes(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = env_probes[probe_index].position_in_grid.w;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).rgb;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_irradiance_buffer, sampler_nearest), probe_uv, 0.0).rgb;
#endif
}

float ReadProbeDepthLowRes(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = env_probes[probe_index].position_in_grid.w;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).r;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_depth_buffer_lowres, sampler_nearest), probe_uv, 0.0).r;
#endif
}

vec3 ComputeLightFieldProbeIrradiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    ivec3 base_probe_position_in_grid;
    const int base_probe_index_indirect = GetLocalEnvProbeIndex(world_position, grid_center, grid_aabb_extent, grid_size, base_probe_position_in_grid);

    if (base_probe_index_indirect >= HYP_MAX_BOUND_LIGHT_FIELD_PROBES || base_probe_index_indirect < 0) {
        return vec3(0.0);
    }


    const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    const ivec3 base_grid_coord = BaseGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);
    vec3 grid_start_position = GridCoordToWorldPosition(base_grid_coord, grid_center, grid_aabb_extent, grid_size);

    if (base_probe_index == ~0u) {
        return vec3(0.0);
    }

    const vec3 base_probe_position = env_probes[base_probe_index].world_position.xyz;

    vec3 alpha = clamp((world_position - grid_start_position) / size_of_probe, vec3(0.0), vec3(1.0));

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

    // iterate over probe cage
    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

        ivec3 neighbor_grid_coord = clamp(base_grid_coord + offset, ivec3(0), ivec3(grid_size) - 1);

        const int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size);
        
        if (neighbor_probe_index_indirect >= HYP_MAX_BOUND_LIGHT_FIELD_PROBES || neighbor_probe_index_indirect < 0) {
            continue;
        }
        
        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

        if (neighbor_probe_index == ~0u) {
            continue;
        }

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = trilinear.x * trilinear.y * trilinear.z;

        const vec3 probe_position_world = env_probes[neighbor_probe_index].world_position.xyz;
        const vec3 probe_to_point = (world_position - probe_position_world);
        const vec3 dir = normalize(-probe_to_point);

        vec3 true_direction_to_probe = normalize(probe_position_world - world_position);
        weight *= max(0.08, dot(dir, N));
        //weight *= HYP_FMATH_SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;

        const uvec3 position_in_grid = uvec3(env_probes[neighbor_probe_index].position_in_grid.xyz);

        const uvec2 probe_offset_coord = uvec2(
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * (position_in_grid.y * grid_size.x + position_in_grid.x),
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * position_in_grid.z
        ) + 1;

        const uvec2 probe_grid_dimensions = uvec2(
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * (grid_size.x * grid_size.y) + 2,
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * grid_size.z + 2
        );

        const vec2 octahedral_dir = EncodeOctahedralCoord(-dir) * 0.5 + 0.5;

#ifdef USE_TEXTURE_ARRAY
        const uint layer_index = (neighbor_probe_index - 1) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

        const vec2 depth_uv = ((octahedral_dir * (PROBE_SIDE_LENGTH_IRRADIANCE - 1)) + 0.5) / vec2(PROBE_SIDE_LENGTH_IRRADIANCE);
        const vec2 moments = texture(sampler2DArray(light_field_filtered_distance_buffer, sampler_linear), vec3(depth_uv, float(layer_index)), 0.0).rg;
#else

        const vec2 depth_uv = (vec2(probe_offset_coord) + (octahedral_dir * PROBE_SIDE_LENGTH_IRRADIANCE) + 0.5) / vec2(probe_grid_dimensions);
        const vec2 moments = Texture2DLod(sampler_linear, light_field_filtered_distance_buffer, depth_uv, 0.0).rg;
#endif

        const float mean = moments.x;
        const float variance = abs(moments.y - HYP_FMATH_SQR(mean));

        const float probe_distance = length(probe_to_point);
        const float distance_minus_mean = probe_distance - mean;

        float chebyshev = variance / (variance + HYP_FMATH_SQR(max(probe_distance - mean, 0.0)));
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (probe_distance <= mean) ? 1.0 : chebyshev;
        weight = max(0.0001, weight);

        const vec3 irradiance_dir = normalize(N);
        vec2 irradiance_uv = EncodeOctahedralCoord(irradiance_dir) * 0.5 + 0.5;

#ifdef USE_TEXTURE_ARRAY
        irradiance_uv = ((irradiance_uv * (PROBE_SIDE_LENGTH_IRRADIANCE - 1)) + 0.5) / vec2(PROBE_SIDE_LENGTH_IRRADIANCE);
        vec3 irradiance = texture(sampler2DArray(light_field_irradiance_buffer, sampler_linear), vec3(irradiance_uv, float(layer_index)), 0.0).rgb;
#else
        irradiance_uv = (vec2(probe_offset_coord) + (irradiance_uv * PROBE_SIDE_LENGTH_IRRADIANCE) + 0.5) / vec2(probe_grid_dimensions);
        vec3 irradiance = Texture2DLod(sampler_linear, light_field_irradiance_buffer, irradiance_uv, 0.0).rgb;
#endif

        // const float crush_threshold = 0.2;
        // if (weight < crush_threshold) {
        //     weight *= weight * weight * (1.0 / HYP_FMATH_SQR(crush_threshold));
        // }

        // irradiance = sqrt(irradiance);

        total_irradiance += irradiance * weight;
        total_weight += weight;
    }

    return 0.5 * HYP_FMATH_PI * total_irradiance / max(total_weight, 0.0001);
}

#endif