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
    vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    vec3 grid_aabb_min = grid_center - (grid_aabb_extent * 0.5);

    return size_of_probe * vec3(grid_coord) + grid_aabb_min;
}

int GridCoordToProbeIndex(ivec3 grid_coord, ivec3 grid_size)
{
    return (int(grid_coord.x) * int(grid_size.y) * int(grid_size.z))
        + (int(grid_coord.y) * int(grid_size.z))
        + int(grid_coord.z) + 1 /* + 1 because the first element is always the reflection probe */;
}

ivec3 BaseGridCoord(vec3 P, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    vec3 grid_aabb_min = grid_center - (grid_aabb_extent * 0.5);
    vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    
    return clamp(ivec3((P - grid_aabb_min) / size_of_probe + 0.5),
        ivec3(0),
        grid_size - ivec3(1));
}

int NearestProbeIndex(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, vec3 world_position)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    vec3 probe_start_position = grid_center - (grid_aabb_extent * 0.5);
    vec3 probe_coords = clamp(round((world_position - probe_start_position) / size_of_probe),
        vec3(0.0),
        vec3(grid_size) - vec3(1.0));

    return GridCoordToProbeIndex(ivec3(probe_coords), grid_size);

    // const ivec3 position_units = ivec3(world_position / size_of_probe + (vec3(grid_size) * 0.5));
    // const ivec3 position_offset = position_units % ivec3(grid_size);

    // const int probe_index_at_point = (int(position_offset.x) * int(grid_size.y) * int(grid_size.z))
    //     + (int(position_offset.y) * int(grid_size.z))
    //     + int(position_offset.z) + 1 /* + 1 because the first element is always the reflection probe */;

    // return probe_index_at_point;
}

/**
    \param neighbors The 8 probes surrounding X
    \return Index into the neighbors array of the index of the nearest probe to X
*/
uint GetNearestProbeIndices(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, vec3 world_position, out uint[8] neighbor_probe_indices)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    vec3 maxProbeCoords = vec3(grid_size) - vec3(1.0);
	vec3 floatProbeCoords = (world_position - grid_center - (grid_aabb_extent * 0.5)) / size_of_probe;
	vec3 baseProbeCoords = clamp(floor(floatProbeCoords), vec3(0.0), maxProbeCoords);

	float minDist = 10.0;
	int nearestIndex = -1;

	for (int i = 0; i < 8; ++i) {
		vec3 newProbeCoords = min(baseProbeCoords + vec3(i & 1, (i >> 1) & 1, (i >> 2) & 1), maxProbeCoords);
		float d = length(newProbeCoords - floatProbeCoords);
		if (d < minDist) {
			minDist = d;
			nearestIndex = i;
		}
	}

	return nearestIndex;

    // const int base_probe_index_indirect = NearestProbeIndex(grid_center, grid_aabb_extent, grid_size, world_position);
    // const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    // if (base_probe_index == ~0u) {
    //     return ~0u;
    // }

    // ivec3 base_grid_coord = BaseGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);

    // float min_distance = 10.0;
    // uint nearest_index = 0;

    // for (int i = 0; i < 8; i++) {
    //     ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

    //     ivec3 neighbor_grid_coord = clamp(base_grid_coord + offset, ivec3(0), ivec3(grid_size) - 1);

    //     int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
    //     uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

    //     neighbor_probe_indices[i] = neighbor_probe_index;

    //     float dist = distance(world_position, env_probes[neighbor_probe_index].world_position.xyz);

    //     if (dist < min_distance) {
    //         min_distance = dist;
    //         nearest_index = uint(i);
    //     }
    // }

    // return nearest_index;
}

int RelativeProbeIndex(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, int baseProbeIndex, int relativeIndex) {
    // // Guaranteed to be a power of 2
    int numProbes = grid_size.x * grid_size.y * grid_size.z;

    ivec3 offset = ivec3(relativeIndex & 1, (relativeIndex >> 1) & 1, (relativeIndex >> 2) & 1);
    ivec3 stride = ivec3(1, grid_size.x, grid_size.x * grid_size.y);

    return (baseProbeIndex + idot(offset, stride)) & (numProbes - 1);

    // ivec3 offset = ivec3(relativeIndex, relativeIndex >> 1, relativeIndex >> 2) & ivec3(1);

    // const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    // ivec3 base_probe_position_in_grid = ivec3(env_probes[baseProbeIndex].position_in_grid.xyz);

    // ivec3 neighbor_grid_coord = clamp(base_probe_position_in_grid + offset, ivec3(0), ivec3(grid_size) - 1);

    // const int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
    // const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

    // return neighbor_probe_index;
}

vec3 GridPositionToWorldPosition(ivec3 pos, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    return size_of_probe * vec3(pos) + (grid_center - (grid_aabb_extent * 0.5));
}

vec3 ProbeLocation(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, uint probe_index)
{
    // const uint real_probe_index = GET_GRID_PROBE_INDEX(probe_index);

    // const ivec3 base_grid_coord = BaseGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);
    // const vec3 base_probe_position_world = GridPositionToWorldPosition(base_grid_coord, grid_center, grid_aabb_extent, grid_size);

    // if (real_probe_index == ~0u) {
    //     return vec3(0.0);
    // }

    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

    return env_probes[probe_index].world_position.xyz;

    // const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    // const ivec3 position_in_grid = ivec3(env_probes[probe_index].position_in_grid.xyz);

    // return size_of_probe * vec3(position_in_grid) + (grid_center - (grid_aabb_extent * 0.5));
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

vec2 TextureCoordFromDirection(vec3 dir, uint probe_index, uvec2 image_dimensions, ivec3 grid_size)
{
    vec2 normalizedOctCoord = EncodeOctahedralCoord(normalize(dir));
    vec2 normalizedOctCoordZeroOne = (normalizedOctCoord + vec2(1.0f)) * 0.5f;

    vec2 octCoordNormalizedToTextureDimensions = (normalizedOctCoordZeroOne * float(PROBE_SIDE_LENGTH_IRRADIANCE)) / vec2(image_dimensions);

    uint probesPerRow = (image_dimensions.x - 2) / uint(PROBE_SIDE_LENGTH_BORDER_IRRADIANCE);

    // border around texture and further 1 pix border around top left probe.
    vec2 probeTopLeftPosition = vec2(mod(probe_index, probesPerRow) * PROBE_SIDE_LENGTH_BORDER_IRRADIANCE,
        (probe_index / probesPerRow) * PROBE_SIDE_LENGTH_BORDER_IRRADIANCE) + vec2(1.0);

    vec2 normalizedProbeTopLeftPosition = vec2(probeTopLeftPosition) / vec2(image_dimensions);

    return vec2(normalizedProbeTopLeftPosition + octCoordNormalizedToTextureDimensions);

    // const uvec3 position_in_grid = uvec3(env_probes[probe_index].position_in_grid.xyz);

    // const uvec2 probe_offset_coord = uvec2(
    //     PROBE_SIDE_LENGTH_BORDER * (position_in_grid.x * grid_size.y + position_in_grid.y),
    //     PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
    // ) + uvec2(2);


    // return vec2(((vec2(probe_offset_coord) + 0.5) / vec2(image_dimensions)) + octCoordNormalizedToTextureDimensions);
}



int GetLocalEnvProbeIndex(vec3 world_position, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, out ivec3 unit_diff)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    const ivec3 position_units = ivec3(world_position / size_of_probe + (vec3(grid_size) * 0.5));
    const ivec3 position_offset = position_units % grid_size;

    unit_diff = position_offset;

    int probe_index_at_point = (int(unit_diff.x) * int(env_grid.density.y) * int(env_grid.density.z))
        + (int(unit_diff.y) * int(env_grid.density.z))
        + int(unit_diff.z) + 1 /* + 1 because the first element is always the reflection probe */;

    return probe_index_at_point;
}

// vec3 ComputeLightFieldProbeRay(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
// {

// }

// vec3 ComputeLightFieldProbeRadiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, inout uint seed)
// {
//     const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

//     ivec3 base_grid_coord;
//     const int base_probe_index_indirect = GetLocalEnvProbeIndex(world_position, grid_center, grid_aabb_extent, grid_size, base_grid_coord) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
//     const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

//     if (base_probe_index == ~0u) {
//         return vec3(0.0);
//     }

//     const vec3 base_probe_position_world = (env_probes[base_probe_index].aabb_max.xyz + env_probes[base_probe_index].aabb_min.xyz) * 0.5;

//     vec3 alpha = clamp((world_position - base_probe_position_world) / size_of_probe, vec3(0.0), vec3(1.0));

//     vec3 total_radiance = vec3(0.0);

//     for (int i = 0; i < HYP_LIGHT_FIELD_PROBE_MAX_RADIANCE_SAMPLES; i++) {
//         vec2 rand_value = vec2(RandomFloat(seed), RandomFloat(seed));

//         vec3 H = ImportanceSampleGGX(rand_value, N, 0.0);
//         vec3 L = reflect(-V, H);

//         vec3 bounce_color = 
//     }

//     return total_radiance / vec3(float(HYP_LIGHT_FIELD_PROBE_MAX_RADIANCE_SAMPLES));
// }

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
    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_color_buffer, sampler_linear), vec3(uv, float(probe_index)), 0.0).rgb;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_color_buffer, sampler_linear), probe_uv, 0.0).rgb;
#endif
}

vec3 ReadProbeNormals(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

#ifdef USE_TEXTURE_ARRAY
    return UnpackNormalVec2(texture(sampler2DArray(light_field_normals_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).rg);
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return UnpackNormalVec2(texture(sampler2D(light_field_normals_buffer, sampler_nearest), probe_uv, 0.0).rg);
#endif
}

float ReadProbeDepth(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).r;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_depth_buffer, sampler_nearest), probe_uv, 0.0).r;
#endif
}

vec3 ReadProbeColorLowRes(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).rgb;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_irradiance_buffer, sampler_nearest), probe_uv, 0.0).rgb;
#endif
}

float ReadProbeDepthLowRes(uint probe_index, vec2 uv, uvec2 image_dimensions, ivec3 grid_size)
{
    probe_index = probe_index % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;

#ifdef USE_TEXTURE_ARRAY
    return texture(sampler2DArray(light_field_depth_buffer, sampler_nearest), vec3(uv, float(probe_index)), 0.0).r;
#else
    const vec2 probe_uv = LocalTexCoordToProbeTexCoord(uv, probe_index, image_dimensions, grid_size);

    return texture(sampler2D(light_field_depth_buffer_lowres, sampler_nearest), probe_uv, 0.0).r;
#endif
}

vec3 ComputeLightFieldProbeRadiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size);

vec3 ComputeLightFieldProbeIrradiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    // return SampleRadiance(world_position, N, V, grid_center, grid_aabb_extent, grid_size);

    // // TEMP test
    // return ComputeLightFieldProbeRadiance(world_position, N, V, grid_center, grid_aabb_extent, grid_size);
    // return TraceRadiance2(world_position, N, V, grid_center, grid_aabb_extent, grid_size);

    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);

    // ivec3 base_probe_position_in_grid;
    // const int base_probe_index_indirect = GetLocalEnvProbeIndex(world_position, grid_center, grid_aabb_extent, grid_size, base_probe_position_in_grid) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
    // const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    // if (base_probe_index == ~0u) {
    //     return vec3(0.0);
    // }

    const ivec3 base_grid_coord = ivec3((world_position - env_grid.aabb_min.xyz) / size_of_probe);  // env_probes[base_probe_index].position_in_grid.xyz;//BaseGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);
    vec3 grid_start_position = vec3(base_grid_coord) * size_of_probe + env_grid.aabb_min.xyz;
    // const ivec3 base_grid_coord = ivec3(env_probes[base_probe_index].position_in_grid.xyz);
    // vec3 grid_start_position = GridCoordToWorldPosition(base_grid_coord, grid_center, grid_aabb_extent, grid_size);
    
    // // const vec3 base_probe_position_world = GridPositionToWorldPosition(base_grid_coord, grid_center, grid_aabb_extent, grid_size);

    // vec3 base_probe_position_world = env_probes[base_probe_index].world_position.xyz;
    vec3 alpha = clamp((world_position - grid_start_position) / size_of_probe, vec3(0.0), vec3(1.0));

    vec3 total_irradiance = vec3(0.0);
    float total_weight = 0.0;

    // iterate over probe cage
    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

        ivec3 neighbor_grid_coord = clamp(base_grid_coord + offset, ivec3(0), ivec3(grid_size) - 1);


        // ivec3 unused;
        // const int neighbor_probe_index_indirect = GetLocalEnvProbeIndex(probe_position_world, grid_center, grid_aabb_extent, grid_size, unused) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
        // const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

        const int neighbor_probe_index_indirect = GridCoordToProbeIndex(neighbor_grid_coord, grid_size) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
        const uint neighbor_probe_index = GET_GRID_PROBE_INDEX(neighbor_probe_index_indirect);

        if (neighbor_probe_index == ~0u) {
            continue;
        }

        const vec3 probe_position_world = env_probes[neighbor_probe_index].aabb_min.xyz;//vec3(neighbor_grid_coord) * size_of_probe + env_grid.aabb_min.xyz;
        const vec3 probe_to_point = world_position - probe_position_world;
        const vec3 dir = normalize(-probe_to_point);

        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight = trilinear.x * trilinear.y * trilinear.z;

        // vec3 true_direction_to_probe = normalize(probe_position_world - world_position);
        weight *= max(0.05, dot(dir, N));
        //weight *= HYP_FMATH_SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;

        const uvec3 position_in_grid = uvec3(env_probes[neighbor_probe_index].position_in_grid.xyz);

        const uvec2 probe_offset_coord_irradiance = uvec2(
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * (position_in_grid.y * grid_size.x + position_in_grid.x),
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * position_in_grid.z
        ) + 1;

        const uvec2 probe_grid_dimensions_irradiance = uvec2(
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
            PROBE_SIDE_LENGTH_BORDER_IRRADIANCE * grid_size.z + PROBE_BORDER_LENGTH
        );

        const uvec2 probe_offset_coord_radiance = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (position_in_grid.y * grid_size.x + position_in_grid.x),
            PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
        ) + 1;

        const uvec2 probe_grid_dimensions_radiance = uvec2(
            PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
            PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
        );

        const vec2 octahedral_dir = EncodeOctahedralCoord(-dir) * 0.5 + 0.5;

#ifdef USE_TEXTURE_ARRAY
        const vec2 depth_uv = ((octahedral_dir * PROBE_SIDE_LENGTH_IRRADIANCE) + 0.5) / vec2(PROBE_SIDE_LENGTH_IRRADIANCE);
        const vec2 moments = texture(sampler2DArray(light_field_depth_buffer, sampler_linear), vec3(depth_uv, float(neighbor_probe_index)), 0.0).rg;
#else

        // const vec2 depth_uv = (vec2(probe_offset_coord_radiance) + (octahedral_dir * PROBE_SIDE_LENGTH_IRRADIANCE)) / vec2(probe_grid_dimensions_radiance);
        const vec2 depth_uv = TextureCoordFromDirection(-dir, neighbor_probe_index_indirect, probe_grid_dimensions_irradiance, grid_size);
        const vec2 moments = Texture2DLod(sampler_linear, light_field_filtered_distance_buffer, depth_uv, 0.0).rg;
#endif

        const float mean = moments.x;
        const float variance = abs(HYP_FMATH_SQR(mean) - moments.y);

        const float probe_distance = length(probe_to_point);
        const float distance_minus_mean = probe_distance - mean;

        float chebyshev = variance / variance + HYP_FMATH_SQR(distance_minus_mean);
        chebyshev = max(HYP_FMATH_CUBE(chebyshev), 0.0);
        weight *= (probe_distance <= mean) ? 1.0 : chebyshev;
        weight = max(0.0001, weight);

        const vec3 irradiance_dir = N;
        vec2 irradiance_uv = EncodeOctahedralCoord(irradiance_dir) * 0.5 + 0.5;

#ifdef USE_TEXTURE_ARRAY
        vec3 irradiance = texture(sampler2DArray(light_field_irradiance_buffer, sampler_linear), vec3(irradiance_uv, float(neighbor_probe_index)), 0.0).rgb;
#else
        irradiance_uv = TextureCoordFromDirection(irradiance_dir, neighbor_probe_index_indirect, probe_grid_dimensions_irradiance, grid_size);
        // irradiance_uv = (vec2(probe_offset_coord_irradiance) + (irradiance_uv * PROBE_SIDE_LENGTH_IRRADIANCE)) / vec2(probe_grid_dimensions_irradiance);
        // vec3 irradiance = Texture2DLod(sampler_linear, light_field_irradiance_buffer, irradiance_uv, 0.0).rgb;

        // const uvec2 probe_offset_coord_debug = uvec2(
        //     PROBE_SIDE_LENGTH_BORDER * (position_in_grid.y * grid_size.x + position_in_grid.x),
        //     PROBE_SIDE_LENGTH_BORDER * position_in_grid.z
        // ) + 1;

        // const uvec2 probe_grid_dimensions_debug = uvec2(
        //     PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        //     PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
        // );
        // const vec2 debug_uv = (vec2(probe_offset_coord_debug) + (octahedral_dir * (PROBE_SIDE_LENGTH - 1)) + 0.5) / vec2(probe_grid_dimensions_debug);
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

    total_irradiance = 0.5 * HYP_FMATH_PI * total_irradiance / max(total_weight, 0.0001);

    return total_irradiance;
    // vec3 net_irradiance = total_irradiance / total_weight;
    // net_irradiance = HYP_FMATH_SQR(net_irradiance);

    // return net_irradiance;
}

bool Trace(
    vec3 grid_center,
    vec3 grid_aabb_extent,
    ivec3 grid_size,
    Ray worldSpaceRay,
    inout float tMax,
    out vec2 hitProbeTexCoord,
    out uint hitProbeIndex,
    const bool fillHoles
);

vec3 ComputeLightFieldProbeRadiance(vec3 world_position, vec3 N, vec3 V, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size)
{
    ivec3 grid_coord = GetGridCoord(world_position, grid_center, grid_aabb_extent, grid_size);
    uint seed = uint(grid_coord.x) * uint(grid_size.y) * uint(grid_size.z) + uint(grid_coord.y) * uint(grid_size.z) + uint(grid_coord.z);
    vec2 rand_value = vec2(RandomFloat(seed), RandomFloat(seed));
    
    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);
    
    vec3 H = ImportanceSampleGGX(rand_value, N, 0.5);
    // H = tangent * H.x + bitangent * H.y + N * H.z;

    // return H;

    const vec3 R = reflect(-V, N);

    Ray ray;
    ray.origin = world_position + N * 0.25;
    ray.direction = R;

    float hit_distance = 10000.0;

    vec2 hit_probe_texcoord;
    uint hit_probe_index = ~0u;

    if (!Trace(grid_center, grid_aabb_extent, grid_size, ray, hit_distance, hit_probe_texcoord, hit_probe_index, false)) {
        return vec3(1.0, 0.0, 0.0);
    }

    uvec2 probe_grid_dimensions = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
    );

    vec3 radiance = ReadProbeColor(hit_probe_index, hit_probe_texcoord, probe_grid_dimensions, grid_size);

    // Render a debug color for the probe using the hit index.

    const vec3 debug_colors[16] = {
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 1.0, 0.0),
        vec3(0.0, 1.0, 1.0),
        vec3(1.0, 0.0, 1.0),
        vec3(1.0, 1.0, 1.0),
        vec3(0.0, 0.0, 0.0),
        vec3(0.5, 0.0, 0.0),
        vec3(0.0, 0.5, 0.0),
        vec3(0.0, 0.0, 0.5),
        vec3(0.5, 0.5, 0.0),
        vec3(0.0, 0.5, 0.5),
        vec3(0.5, 0.0, 0.5),
        vec3(0.5, 0.5, 0.5),
        vec3(0.0, 0.0, 0.0)
    };

    // return debug_colors[hit_probe_index % 16];

    return radiance;
}

/** Segments a ray into the piecewise-continuous rays or line segments that each lie within
    one Euclidean octant, which correspond to piecewise-linear projections in octahedral space.

    \param boundaryT  all boundary distance ("time") values in units of world-space distance
      along the ray. In the (common) case where not all five elements are needed, the unused
      values are all equal to tMax, creating degenerate ray segments.

    \param origin Ray origin in the Euclidean object space of the probe

    \param directionFrac 1 / ray.direction
 */
void ComputeRaySegments(vec3 origin, vec3 directionFrac, float tMin, float tMax, out float boundaryTs[5])
{
    boundaryTs[0] = tMin;

    // Time values for intersection with x = 0, y = 0, and z = 0 planes, sorted
    // in increasing order
    vec3 t = origin * -directionFrac;
    Sort(t);

    // Copy the values into the interval boundaries.
    // This loop expands at compile time and eliminates the
    // relative indexing, so it is just three conditional move operations
    for (int i = 0; i < 3; ++i) {
        boundaryTs[i + 1] = clamp(t[i], tMin, tMax);
    }

    boundaryTs[4] = tMax;
}

float DistanceToIntersection(in Ray R, vec3 v) {
    float numer;
    float denom = v.y * R.direction.z - v.z * R.direction.y;

    if (abs(denom) > 0.1) {
        numer = R.origin.y * R.direction.z - R.origin.z * R.direction.y;
    }
    else {
        // We're in the yz plane; use another one
        numer = R.origin.x * R.direction.y - R.origin.y * R.direction.x;
        denom = v.x * R.direction.y - v.y * R.direction.x;
    }

    return numer / denom;
}

TraceResult HighResolutionTraceOneRaySegment(
    vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size,
    in Ray probeSpaceRay, in vec2 startTexCoord,
    in vec2 endTexCoord, in uint probeIndex,
    inout float tMin, inout float tMax,
    inout vec2 hitProbeTexCoord)
{
    const uvec2 probe_grid_dimensions = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
    );

    vec2 texCoordDelta = endTexCoord - startTexCoord;
    float texCoordDistance = length(texCoordDelta);
    vec2 texCoordDirection = texCoordDelta * (1.0 / texCoordDistance);

    float texCoordStep = INV_TEX_SIZE.x * (texCoordDistance / MaxComponent(abs(texCoordDelta)));

    vec3 directionFromProbeBefore = DecodeOctahedralCoord(startTexCoord * 2.0 - 1.0);
    float distanceFromProbeToRayBefore = max(0.0, DistanceToIntersection(probeSpaceRay, directionFromProbeBefore));


    for (float d = 0.0f; d <= texCoordDistance; d += texCoordStep) {
        vec2 texCoord = (texCoordDirection * min(d + texCoordStep * 0.5, texCoordDistance)) + startTexCoord;

        // Fetch the probe data
        float distanceFromProbeToSurface = ReadProbeDepth(probeIndex, texCoord, probe_grid_dimensions, grid_size);

        // Find the corresponding point in probe space. This defines a line through the 
        // probe origin
        vec3 directionFromProbe = DecodeOctahedralCoord(texCoord * 2.0 - 1.0);

        vec2 texCoordAfter = (texCoordDirection * min(d + texCoordStep, texCoordDistance)) + startTexCoord;
        vec3 directionFromProbeAfter = DecodeOctahedralCoord(texCoordAfter * 2.0 - 1.0);
        float distanceFromProbeToRayAfter = max(0.0, DistanceToIntersection(probeSpaceRay, directionFromProbeAfter));
        float maxDistFromProbeToRay = max(distanceFromProbeToRayBefore, distanceFromProbeToRayAfter);

        if (maxDistFromProbeToRay >= distanceFromProbeToSurface) {
            // At least a one-sided hit; see if the ray actually passed through the surface, or was behind it

            float minDistFromProbeToRay = min(distanceFromProbeToRayBefore, distanceFromProbeToRayAfter);

            // Find the 3D point *on the trace ray* that corresponds to the tex coord.
            // This is the intersection of the ray out of the probe origin with the trace ray.
            float distanceFromProbeToRay = (minDistFromProbeToRay + maxDistFromProbeToRay) * 0.5;

            // Use probe information
            vec3 probeSpaceHitPoint = distanceFromProbeToSurface * directionFromProbe;
            float distAlongRay = dot(probeSpaceHitPoint - probeSpaceRay.origin, probeSpaceRay.direction);

            // Read the normal for use in detecting backfaces
            vec3 normal = ReadProbeNormals(probeIndex, texCoord, probe_grid_dimensions, grid_size);
            // Only extrude towards and away from the view ray, not perpendicular to it
            // Don't allow extrusion TOWARDS the viewer, only away
            float surfaceThickness = minThickness
                + (maxThickness - minThickness) *

                // Alignment of probe and view ray
                max(dot(probeSpaceRay.direction, directionFromProbe), 0.0) *

                // Alignment of probe and normal (glancing surfaces are assumed to be thicker because they extend into the pixel)
                (2 - abs(dot(probeSpaceRay.direction, normal))) *

                // Scale with distance along the ray
                clamp(distAlongRay * 0.1, 0.05, 1.0);


            if ((minDistFromProbeToRay < distanceFromProbeToSurface + surfaceThickness) && (dot(normal, probeSpaceRay.direction) < 0)) {
                // Two-sided hit
                // Use the probe's measure of the point instead of the ray distance, since
                // the probe is more accurate (floating point precision vs. ray march iteration/oct resolution)
                tMax = distAlongRay;
                hitProbeTexCoord = texCoord;

                return TRACE_RESULT_HIT;
            } else {
                // "Unknown" case. The ray passed completely behind a surface. This should trigger moving to another
                // probe and is distinguished from "I successfully traced to infinity"

                // Back up conservatively so that we don't set tMin too large
                vec3 probeSpaceHitPointBefore = distanceFromProbeToRayBefore * directionFromProbeBefore;
                float distAlongRayBefore = dot(probeSpaceHitPointBefore - probeSpaceRay.origin, probeSpaceRay.direction);

                // Max in order to disallow backing up along the ray (say if beginning of this texel is before tMin from probe switch)
                // distAlongRayBefore in order to prevent overstepping
                // min because sometimes distAlongRayBefore > distAlongRay
                tMin = max(tMin, min(distAlongRay, distAlongRayBefore));

                return TRACE_RESULT_UNKNOWN;
            }
        }
        distanceFromProbeToRayBefore = distanceFromProbeToRayAfter;
    } // ray march

    return TRACE_RESULT_MISS;
}

/** Returns true on a conservative hit, false on a guaranteed miss.
    On a hit, advances lowResTexCoord to the next low res texel *after*
    the one that produced the hit.

    The texture coordinates are not texel centers...they are sub-texel
    positions true to the actual ray. This allows chopping up the ray
    without distorting it.

    segmentEndTexCoord is the coordinate of the endpoint of the entire segment of the ray

    texCoord is the start coordinate of the segment crossing
    the low-res texel that produced the conservative hit, if the function
    returns true.  endHighResTexCoord is the end coordinate of that
    segment...which is also the start of the NEXT low-res texel to cross
    when resuming the low res trace.

  */
bool LowResolutionTraceOneSegment(
    vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size,
    in Ray probeSpaceRay,
    in uint probeIndex,
    inout vec2 texCoord,
    in vec2 segmentEndTexCoord,
    inout vec2 endHighResTexCoord
)
{
    const uvec2 probe_grid_dimensions = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
    );

    vec2 lowResSize = TEX_SIZE_SMALL;//size(lightFieldSurface.lowResolutionDistanceProbeGrid);
    vec2 lowResInvSize = INV_TEX_SIZE_SMALL;//invSize(lightFieldSurface.lowResolutionDistanceProbeGrid);

    // Convert the texels to pixel coordinates:
    vec2 P0 = texCoord * lowResSize;
    vec2 P1 = segmentEndTexCoord * lowResSize;

    // If the line is degenerate, make it cover at least one pixel
    // to avoid handling zero-pixel extent as a special case later
    P1 += vec2((DistanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0);
    // In pixel coordinates
    vec2 delta = P1 - P0;

    // Permute so that the primary iteration is in x to reduce
    // large branches later
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        // This is a more-vertical line
        permute = true;
        delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
    }

    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;
    vec2 dP = vec2(stepDir, delta.y * invdx);

    vec3 initialDirectionFromProbe = DecodeOctahedralCoord(texCoord * 2.0 - 1.0);
    float prevRadialDistMaxEstimate = max(0.0, DistanceToIntersection(probeSpaceRay, initialDirectionFromProbe));
    // Slide P from P0 to P1
    float end = P1.x * stepDir;

    float absInvdPY = 1.0 / abs(dP.y);

    // Don't ever move farther from texCoord than this distance, in texture space,
    // because you'll move past the end of the segment and into a different projection
    float maxTexCoordDistance = LengthSquared(segmentEndTexCoord - texCoord);

    for (vec2 P = P0; ((P.x * sign(delta.x)) <= end); ) {
        vec2 hitPixel = permute ? P.yx : P;

        float sceneRadialDistMin = ReadProbeDepthLowRes(probeIndex, hitPixel * INV_TEX_SIZE_SMALL, probe_grid_dimensions, grid_size);

        // Distance along each axis to the edge of the low-res texel
        vec2 intersectionPixelDistance = (sign(delta) * 0.5 + 0.5) - sign(delta) * fract(P);

        // abs(dP.x) is 1.0, so we skip that division
        // If we are parallel to the minor axis, the second parameter will be inf, which is fine
        float rayDistanceToNextPixelEdge = min(intersectionPixelDistance.x, intersectionPixelDistance.y * absInvdPY);

        // The exit coordinate for the ray (this may be *past* the end of the segment, but the
        // callr will handle that)
        endHighResTexCoord = (P + dP * rayDistanceToNextPixelEdge) * lowResInvSize;
        endHighResTexCoord = permute ? endHighResTexCoord.yx : endHighResTexCoord;

        if (LengthSquared(endHighResTexCoord - texCoord) > maxTexCoordDistance) {
            // Clamp the ray to the segment, because if we cross a segment boundary in oct space
            // then we bend the ray in probe and world space.
            endHighResTexCoord = segmentEndTexCoord;
        }

        // Find the 3D point *on the trace ray* that corresponds to the tex coord.
        // This is the intersection of the ray out of the probe origin with the trace ray.
        vec3 directionFromProbe = DecodeOctahedralCoord(endHighResTexCoord * 2.0 - 1.0);
        float distanceFromProbeToRay = max(0.0, DistanceToIntersection(probeSpaceRay, directionFromProbe));

        float maxRadialRayDistance = max(distanceFromProbeToRay, prevRadialDistMaxEstimate);
        prevRadialDistMaxEstimate = distanceFromProbeToRay;

        if (sceneRadialDistMin < maxRadialRayDistance) {
            // A conservative hit.
            //
            //  -  endHighResTexCoord is already where the ray would have LEFT the texel
            //     that created the hit.
            //
            //  -  texCoord should be where the ray entered the texel
            texCoord = (permute ? P.yx : P) * lowResInvSize;
            return true;
        }

        // Ensure that we step just past the boundary, so that we're slightly inside the next
        // texel, rather than at the boundary and randomly rounding one way or the other.
        const float epsilon = 0.001; // pixels
        P += dP * (rayDistanceToNextPixelEdge + epsilon);
    } // for each pixel on ray

    // If exited the loop, then we went *past* the end of the segment, so back up to it (in practice, this is ignored
    // by the caller because it indicates a miss for the whole segment)
    texCoord = segmentEndTexCoord;

    return false;
}


TraceResult TraceOneRaySegment
   (vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size,
    in Ray probeSpaceRay,
    in float t0,
    in float t1,
    in uint probeIndex,
    inout float tMin, // out only
    inout float tMax,
    inout vec2 hitProbeTexCoord)
{

    // Euclidean probe-space line segment, composed of two points on the probeSpaceRay
    vec3 probeSpaceStartPoint = probeSpaceRay.origin + probeSpaceRay.direction * (t0 + rayBumpEpsilon);
    vec3 probeSpaceEndPoint = probeSpaceRay.origin + probeSpaceRay.direction * (t1 - rayBumpEpsilon);

    // If the original ray origin is really close to the probe origin, then probeSpaceStartPoint will be close to zero
    // and we get NaN when we normalize it. One common case where this can happen is when the camera is at the probe
    // center. (The end point is also potentially problematic, but the chances of the end landing exactly on a probe
    // are relatively low.) We only need the *direction* to the start point, and using probeSpaceRay.direction
    // is safe in that case.
    if (LengthSquared(probeSpaceStartPoint) < 0.001) {
        probeSpaceStartPoint = probeSpaceRay.direction;
    }

    // Corresponding octahedral ([-1, +1]^2) space line segment.
    // Because the points are in probe space, we don't have to subtract off the probe's origin
    vec2 startOctCoord = EncodeOctahedralCoord(normalize(probeSpaceStartPoint));
    vec2 endOctCoord = EncodeOctahedralCoord(normalize(probeSpaceEndPoint));

    // Texture coordinates on [0, 1]
    vec2 texCoord = startOctCoord * 0.5 + 0.5;
    vec2 segmentEndTexCoord = endOctCoord * 0.5 + 0.5;

    const int max_iterations = 150;

    for (int i = 0; i < max_iterations; ++i) {
        vec2 endTexCoord;

        // Trace low resolution, min probe until we:
        // - reach the end of the segment (return "miss" from the whole function)
        // - "hit" the surface (invoke high-resolution refinement, and then iterate if *that* misses)

        // If lowResolutionTraceOneSegment conservatively "hits", it will set texCoord and endTexCoord to be the high-resolution texture coordinates.
        // of the intersection between the low-resolution texel that was hit and the ray segment.
        vec2 originalStartCoord = texCoord;
        if (!LowResolutionTraceOneSegment(grid_center, grid_aabb_extent, grid_size, probeSpaceRay, probeIndex, texCoord, segmentEndTexCoord, endTexCoord)) {
            // The whole trace failed to hit anything
            return TRACE_RESULT_MISS;
        } else {

            // The low-resolution trace already guaranted that endTexCoord is no farther along the ray than segmentEndTexCoord if this point is reached,
            // so we don't need to clamp to the segment length
            TraceResult result = HighResolutionTraceOneRaySegment(grid_center, grid_aabb_extent, grid_size, probeSpaceRay, texCoord, endTexCoord, probeIndex, tMin, tMax, hitProbeTexCoord);

            if (result != TRACE_RESULT_MISS) {
                // High-resolution hit or went behind something, which must be the result for the whole segment trace
                return result;
            }

        } // else...continue the outer loop; we conservatively refined and didn't actually find a hit

        // Recompute each time around the loop to avoid increasing the peak register count
        vec2 texCoordRayDirection = normalize(segmentEndTexCoord - texCoord);

        if (dot(texCoordRayDirection, segmentEndTexCoord - endTexCoord) <= /*invSize(lightFieldSurface.distanceProbeGrid)*/ INV_TEX_SIZE.x) {
            // The high resolution trace reached the end of the segment; we've failed to find a hit
            return TRACE_RESULT_MISS;
        } else {
            // We made it to the end of the low-resolution texel using the high-resolution trace, so that's
            // the starting point for the next low-resolution trace. Bump the ray to guarantee that we advance
            // instead of getting stuck back on the low-res texel we just verified...but, if that fails on the
            // very first texel, we'll want to restart the high-res trace exactly where we left off, so
            // don't bump by an entire high-res texel
            texCoord = endTexCoord + texCoordRayDirection * /*invSize(lightFieldSurface.distanceProbeGrid)*/ INV_TEX_SIZE.x * 0.1;
        }
    } // while low-resolution trace

    // Reached the end of the segment
    return TRACE_RESULT_MISS;
}



/**
  \param tMax On call, the stop distance for the trace. On return, the distance
        to the new hit, if one was found. Always finite.
  \param tMin On call, the start distance for the trace. On return, the start distance
        of the ray right before the first "unknown" step.
  \param hitProbeTexCoord Written to only on a hit
  \param index probe index
 */
TraceResult TraceOneProbeOct(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, uint probe_index, in Ray worldSpaceRay, inout float tMin, inout float tMax, inout vec2 hitProbeTexCoord)
{
    // How short of a ray segment is not worth tracing?
    const float degenerateEpsilon = 0.001; // meters

    vec3 probeOrigin = ProbeLocation(grid_center, grid_aabb_extent, grid_size, probe_index);
    //Point3 probeOrigin = Point3(-10.0, 4.0, 0.0);// @Simplification

    Ray probeSpaceRay;
    probeSpaceRay.origin = worldSpaceRay.origin - probeOrigin;
    probeSpaceRay.direction = worldSpaceRay.direction;

    // Maximum of 5 boundary points when projecting ray onto octahedral map;
    // ray origin, ray end, intersection with each of the XYZ planes.
    float boundaryTs[5];
    ComputeRaySegments(probeSpaceRay.origin, vec3(1.0) / probeSpaceRay.direction, tMin, tMax, boundaryTs);

    // for each open interval (t[i], t[i + 1]) that is not degenerate
    for (int i = 0; i < 4; ++i) {
        if (abs(boundaryTs[i] - boundaryTs[i + 1]) >= degenerateEpsilon) {
            TraceResult result = TraceOneRaySegment(grid_center, grid_aabb_extent, grid_size, probeSpaceRay, boundaryTs[i], boundaryTs[i + 1], probe_index, tMin, tMax, hitProbeTexCoord);

            if (result == TRACE_RESULT_HIT) {
                // Hit!
                return TRACE_RESULT_HIT;
            }

            if (result == TRACE_RESULT_UNKNOWN) {
                // Failed to find anything conclusive
                return TRACE_RESULT_UNKNOWN;
            }

        } // if
    } // For each segment

    return TRACE_RESULT_MISS;
}


/** Traces a ray against the full lightfield.
    Returns true on a hit and updates \a tMax if there is a ray hit before \a tMax.
   Otherwise returns false and leaves tMax unmodified

   \param hitProbeTexCoord on [0, 1]

   \param fillHoles If true, this function MUST return a hit even if it is forced to use a coarse approximation
 */
bool Trace(vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size,
    Ray worldSpaceRay, inout float tMax, out vec2 hitProbeTexCoord, out uint hitProbeIndex, const bool fillHoles)
{
    const uvec2 probe_grid_dimensions = uvec2(
        PROBE_SIDE_LENGTH_BORDER * (grid_size.x * grid_size.y) + PROBE_BORDER_LENGTH,
        PROBE_SIDE_LENGTH_BORDER * grid_size.z + PROBE_BORDER_LENGTH
    );

    hitProbeIndex = ~0u;

    // ivec3 base_grid_coord;
    // const int base_probe_index_indirect = GetLocalEnvProbeIndex(worldSpaceRay.origin, grid_center, grid_aabb_extent, grid_size, base_grid_coord) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
    // const uint base_probe_index = GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    // if (base_probe_index == ~0u) {
    //     return false;
    // }

    int base_probe_index_indirect = NearestProbeIndex(grid_center, grid_aabb_extent, grid_size, worldSpaceRay.origin);
    const uint base_probe_index = 0;//GET_GRID_PROBE_INDEX(base_probe_index_indirect);

    uint probe_indices[8];
    uint i = GetNearestProbeIndices(grid_center, grid_aabb_extent, grid_size, worldSpaceRay.origin, probe_indices);

    int probesLeft = 8;
    float tMin = 0.0;

    // hitProbeIndex = NearestProbeIndex(grid_center, grid_aabb_extent, grid_size, worldSpaceRay.origin);
    // hitProbeIndex = GET_GRID_PROBE_INDEX(hitProbeIndex);
    // hitProbeTexCoord = EncodeOctahedralCoord(worldSpaceRay.direction) * 0.5 + 0.5;

    // float probeDistance = ReadProbeDepth(hitProbeIndex, hitProbeTexCoord, probe_grid_dimensions, grid_size);

    // if (probeDistance < 10000.0) {
    //     vec3 hitLocation = ProbeLocation(grid_center, grid_aabb_extent, grid_size, hitProbeIndex) + worldSpaceRay.direction * probeDistance;
    //     tMax = length(worldSpaceRay.origin - hitLocation);
    //     return true;
    // }

    while (probesLeft > 0) {
        int probe_index_indirect = RelativeProbeIndex(grid_center, grid_aabb_extent, grid_size, int(base_probe_index_indirect), int(i));
        uint probe_index = GET_GRID_PROBE_INDEX(probe_index_indirect);

        TraceResult result = TraceOneProbeOct(
            grid_center, grid_aabb_extent, grid_size,
            probe_index,
            worldSpaceRay,
            tMin, tMax,
            hitProbeTexCoord
        );
        
        if (result == TRACE_RESULT_UNKNOWN) {
            i = NEXT_CYCLE_INDEX(i);
            --probesLeft;
        } else {
            if (result == TRACE_RESULT_HIT) {
                int hit_probe_index_indirect = RelativeProbeIndex(grid_center, grid_aabb_extent, grid_size, int(base_probe_index_indirect), int(i));
                hitProbeIndex = GET_GRID_PROBE_INDEX(hit_probe_index_indirect);
            }
            // Found the hit point
            break;
        }
    }
    return (hitProbeIndex != ~0u);
}

#endif