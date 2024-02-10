#ifndef ENV_GRID_RADIANCE_GLSL
#define ENV_GRID_RADIANCE_GLSL

#include "../include/brdf.inc"
#include "../include/noise.inc"
#include "../include/BlueNoise.glsl"
#include "../include/Octahedron.glsl"

#define HYP_VCT_USE_ROUGHNESS_SCATTERING

vec4 FetchVoxel(vec3 position, float lod)
{
    vec4 rgba = textureLod(sampler3D(voxel_image, sampler_linear), position, lod);
    // rgba.rgb = ReverseTonemapReinhardSimple(rgba.rgb);
    return rgba;
}

vec3 VctWorldToTexCoord(vec3 world_position, in AABB voxel_grid_aabb)
{
    const vec3 voxel_grid_aabb_min = vec3(min(voxel_grid_aabb.min.x, min(voxel_grid_aabb.min.y, voxel_grid_aabb.min.z)));
    const vec3 voxel_grid_aabb_max = vec3(max(voxel_grid_aabb.max.x, max(voxel_grid_aabb.max.y, voxel_grid_aabb.max.z)));
    const vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    const vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    const vec3 scaled_position = (world_position - voxel_grid_aabb_center) / voxel_grid_aabb_extent;
    const vec3 voxel_storage_position = (scaled_position * 0.5 + 0.5);

    return clamp(voxel_storage_position, vec3(0.0), vec3(1.0));
}

vec4 ConeTrace(float voxel_size, vec3 origin, vec3 dir, float ratio, float max_dist, bool include_lighting)
{
    const float min_diameter = voxel_size;
    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum = vec4(0.0);
    vec3 sample_pos = origin;
    float diameter = min_diameter;//max(min_diameter, dist * ratio);
    float dist = 0.0;//min_diameter;

    uint iter = 0;
    const uint max_iterations = 800;

    while (dist < max_dist && accum.a < 1.0 && iter < max_iterations) {
        float lod = log2(diameter * min_diameter_inv);

        sample_pos = origin + dir * dist;

        vec4 voxel_color = FetchVoxel(sample_pos, lod);
        voxel_color.rgb *= mix(1.0, voxel_color.a, float(include_lighting));
        voxel_color.rgb *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;

        const float prev_dist = dist;
        dist += diameter;
        diameter = max(dist * ratio, min_diameter);

        iter++;
    }
    
    return accum;
}

vec4 ConeTraceSpecular(vec3 P, vec3 N, vec3 R, vec3 V, float roughness, in AABB voxel_grid_aabb)
{
    // if (roughness >= 1.0) {
    //     return vec4(0.0);
    // }

    const vec3 voxel_coord = VctWorldToTexCoord(P, voxel_grid_aabb);
    const float greatest_extent = 256.0;
    const float voxel_size = 1.0 / greatest_extent;

    return ConeTrace(voxel_size, voxel_coord + N * max(0.01, voxel_size), R, RoughnessToConeAngle(roughness), 0.65, false);
}

vec4 ComputeVoxelRadiance(vec3 world_position, vec3 N, vec3 V, float roughness, uvec2 pixel_coord, uvec2 screen_resolution, uint frame_counter, ivec3 grid_size, in AABB voxel_grid_aabb)
{
    roughness = clamp(roughness, 0.01, 0.99);

#ifdef HYP_VCT_USE_ROUGHNESS_SCATTERING
    vec2 blue_noise_sample = vec2(
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    );

    vec2 blue_noise_scaled = blue_noise_sample + float(frame_counter % 256) * 1.618;
    vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    vec3 H = ImportanceSampleGGX(rnd, N, roughness);
    H = tangent * H.x + bitangent * H.y + N * H.z;

    const vec3 R = normalize(reflect(-V, H));
#else
    const vec3 R = normalize(reflect(-V, N));
#endif

    return ConeTraceSpecular(world_position, N, R, V, roughness, voxel_grid_aabb);
}

bool RayTraceLocal(vec3 P, vec3 N, vec3 R, float roughness, ivec3 grid_size, in AABB aabb, out vec4 hit_color)
{
    vec3 aabb_center = (aabb.min + aabb.max) * 0.5;
    vec3 aabb_extent = aabb.max - aabb.min;

    ivec3 _unused;
    int probe_index_indirect = GetLocalEnvProbeIndex(P, aabb_center, aabb_extent, grid_size, _unused);

    if (probe_index_indirect < 0 || probe_index_indirect >= HYP_MAX_ENV_PROBES) {
        return false;
    }

    uint probe_index = GET_GRID_PROBE_INDEX(probe_index_indirect);

    if (probe_index == ~0u) {
        return false;
    }

    const vec3 probe_position_world = env_probes[probe_index].world_position.xyz;
    const vec3 probe_to_point = P - probe_position_world;
    const vec3 dir = normalize(-probe_to_point);

    const uvec2 probe_grid_dimensions = uvec2(
        (256 + 2) * (grid_size.x * grid_size.y) + 2,
        (256 + 2) * grid_size.z + 2
    );

#ifdef USE_TEXTURE_ARRAY
    const uint layer_index = env_probes[probe_index].position_in_grid.w % HYP_MAX_BOUND_LIGHT_FIELD_PROBES;
#endif

    const vec2 octahedral_dir = EncodeOctahedralCoord(-dir) * 0.5 + 0.5;

#ifdef USE_TEXTURE_ARRAY
    const vec2 depth_uv = ((octahedral_dir * (256 - 1)) + 0.5) / vec2(256);
    const float dist = texture(sampler2DArray(light_field_depth_buffer, sampler_linear), vec3(depth_uv, float(layer_index)), 0.0).r;
#else

    const vec2 depth_uv = (vec2(probe_offset_coord) + (octahedral_dir * 256) + 0.5) / vec2(probe_grid_dimensions);
    // const vec2 depth_uv = TextureCoordFromDirection(-dir, neighbor_probe_index, probe_grid_dimensions, grid_size);
    const float dist = Texture2DLod(sampler_linear, light_field_depth_buffer, depth_uv, 0.0).r;
#endif

    // if (dist < 0.0001 || dist > 100.0) {
    //     return false;
    // }

    vec3 radiance = texture(sampler2DArray(light_field_color_buffer, sampler_linear), vec3(octahedral_dir, float(layer_index)), 0.0).rgb;

    // Test
    hit_color = vec4(radiance, 1.0);

    return true;
}

vec4 RayTrace(vec3 P, vec3 N, vec3 R, float roughness, ivec3 grid_size, in AABB aabb)
{
    const int max_iterations = 16;

    const vec3 size_of_probe = (aabb.max - aabb.min) / vec3(grid_size);
    const vec3 step_size = size_of_probe * 0.5;

    for (int i = 0; i < max_iterations; ++i) {
        vec4 hit_color;

        if (RayTraceLocal(P, N, R, roughness, grid_size, aabb, hit_color)) {
            return hit_color;
        }

        P += step_size * R;
    }

    return vec4(0.0);
}

vec4 ComputeProbeReflection(vec3 world_position, vec3 N, vec3 V, float roughness, uvec2 pixel_coord, uvec2 screen_resolution, uint frame_counter, ivec3 grid_size, in AABB aabb)
{
    roughness = clamp(roughness, 0.01, 0.99);

    vec2 blue_noise_sample = vec2(
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    );

    vec2 blue_noise_scaled = blue_noise_sample + float(frame_counter % 256) * 1.618;
    vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));

    // uint seed = pcg_hash(pixel_coord.x + pixel_coord.y * screen_resolution.x);
    // vec2 rnd = vec2(RandomFloat(seed), RandomFloat(seed));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    vec3 H = ImportanceSampleGGX(rnd, N, roughness);
    H = tangent * H.x + bitangent * H.y + N * H.z;

    const vec3 R = normalize(reflect(-V, H));

    return RayTrace(world_position, N, R, roughness, grid_size, aabb);
}

#endif // ENV_GRID_RADIANCE_GLSL