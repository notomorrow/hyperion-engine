#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#ifdef MODE_IRRADIANCE
#define MODE 0
#elif defined(MODE_RADIANCE)
#define MODE 1
#else
#define MODE 0
#endif

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/aabb.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp

#include "./DeferredLighting.glsl"
#include "../include/env_probe.inc"

#define USE_TEXTURE_ARRAY

#ifdef USE_TEXTURE_ARRAY
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 63) uniform texture2DArray light_field_color_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 64) uniform texture2DArray light_field_normals_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 65) uniform texture2DArray light_field_depth_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 66) uniform texture2DArray light_field_depth_buffer_lowres;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 67) uniform texture2DArray light_field_irradiance_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 68) uniform texture2DArray light_field_filtered_distance_buffer;
#else
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 63) uniform texture2D light_field_color_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 64) uniform texture2D light_field_normals_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 65) uniform texture2D light_field_depth_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 66) uniform texture2D light_field_depth_buffer_lowres;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 67) uniform texture2D light_field_irradiance_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 68) uniform texture2D light_field_filtered_distance_buffer;
#endif

int GetLocalEnvProbeIndex(vec3 world_position, vec3 grid_center, vec3 grid_aabb_extent, ivec3 grid_size, out ivec3 unit_diff)
{
    const vec3 size_of_probe = grid_aabb_extent / vec3(grid_size);
    const ivec3 position_units = ivec3(world_position / size_of_probe + (vec3(grid_size) * 0.5));
    const ivec3 position_offset = position_units % grid_size;

    unit_diff = position_offset;

    int probe_index_at_point = (int(unit_diff.x) * int(env_grid.density.y) * int(env_grid.density.z))
        + (int(unit_diff.y) * int(env_grid.density.z))
        + int(unit_diff.z);

    return probe_index_at_point;
}

#if MODE == 0
#include "../light_field/ComputeIrradiance.glsl"
#elif MODE == 1
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 69) uniform texture3D voxel_image;

#include "./EnvGridRadiance.glsl"
#endif

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

int CubeRoot(int x)
{
    return int(round(pow(float(x), 1.0 / 3.0)));
}

void main()
{
    vec3 irradiance = vec3(0.0);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = normalize(DecodeNormal(SampleGBuffer(gbuffer_ws_normals_texture, v_texcoord)));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);

#if MODE == 0

#if 1
    irradiance = CalculateEnvProbeIrradiance(P, N);

    color_output = vec4(irradiance, 1.0);
#else
    irradiance = ComputeLightFieldProbeIrradiance(P, N, V, env_grid.center.xyz, env_grid.aabb_extent.xyz, ivec3(env_grid.density.xyz));

    color_output = vec4(irradiance, 1.0);
#endif

#else // Radiance
    const vec4 material = SampleGBuffer(gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    uvec2 pixel_coord = uvec2(v_texcoord * vec2(camera.dimensions.xy) - 1.0);
    uvec2 screen_resolution = uvec2(camera.dimensions.xy);
    uint frame_counter = scene.frame_counter;

    AABB voxel_grid_aabb;
    voxel_grid_aabb.min = env_grid.voxel_grid_aabb_min.xyz;
    voxel_grid_aabb.max = env_grid.voxel_grid_aabb_max.xyz;

    // vec4 radiance = ComputeProbeReflection(P, N, V, roughness, pixel_coord, screen_resolution, frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb);
    vec4 radiance = ComputeVoxelRadiance(P, N, V, roughness, pixel_coord, screen_resolution, frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb);

    color_output = radiance;
#endif
}