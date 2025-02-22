#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../include/scene.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/brdf.inc"
#include "../include/aabb.inc"

#define ENV_GRID_IRRADIANCE_MODE_SH 0
#define ENV_GRID_IRRADIANCE_MODE_VOXEL 1
#define ENV_GRID_IRRADIANCE_MODE_LIGHT_FIELD 2

#define ENV_GRID_IRRADIANCE_MODE ENV_GRID_IRRADIANCE_MODE_LIGHT_FIELD //ENV_GRID_IRRADIANCE_MODE_SH

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "./DeferredLighting.glsl"

#if defined(MODE_RADIANCE) || ENV_GRID_IRRADIANCE_MODE == ENV_GRID_IRRADIANCE_MODE_VOXEL
HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer) readonly buffer BlueNoiseBuffer
{
	ivec4 sobol_256spp_256d[256 * 256 / 4];
	ivec4 scrambling_tile[128 * 128 * 8 / 4];
	ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_SRV(Scene, VoxelGridTexture) uniform texture3D voxel_image;

#include "./EnvGridVoxelConeTracing.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

void main()
{
    uvec2 screen_resolution = uvec2(deferred_params.screen_width, deferred_params.screen_height) / 2;
    uvec2 pixel_coord = uvec2(v_texcoord * (vec2(screen_resolution) - 1.0));

// #if defined(MODE_RADIANCE) || ENV_GRID_IRRADIANCE_MODE == ENV_GRID_IRRADIANCE_MODE_VOXEL
//     // Checkerboard rendering
//     const uint pixel_index = pixel_coord.y * screen_resolution.x + pixel_coord.x;

//     if (bool(((pixel_coord.x & 1) ^ (pixel_coord.y & 1) ^ (scene.frame_counter & 1))))
//     {
//         color_output = vec4(0.0);
//         return;
//     }
// #endif

    vec3 irradiance = vec3(0.0);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = DecodeNormal(Texture2D(sampler_nearest, gbuffer_ws_normals_texture, v_texcoord));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);

#if defined(MODE_RADIANCE) || ENV_GRID_IRRADIANCE_MODE == ENV_GRID_IRRADIANCE_MODE_VOXEL
    const vec4 material = Texture2D(sampler_nearest, gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    AABB voxel_grid_aabb;
    voxel_grid_aabb.min = env_grid.voxel_grid_aabb_min.xyz;
    voxel_grid_aabb.max = env_grid.voxel_grid_aabb_max.xyz;
#endif

#if defined(MODE_IRRADIANCE)
#if ENV_GRID_IRRADIANCE_MODE == ENV_GRID_IRRADIANCE_MODE_VOXEL
    irradiance = ComputeVoxelIrradiance(P, N, pixel_coord, screen_resolution, scene.frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb).rgb;
#else
    irradiance = CalculateEnvGridIrradiance(P, N, V);
#endif

    color_output = vec4(irradiance, 1.0);
#elif defined(MODE_RADIANCE)
    vec4 radiance = ComputeVoxelRadiance(P, N, V, roughness, pixel_coord, screen_resolution, scene.frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb);

    color_output = radiance;
#endif
}