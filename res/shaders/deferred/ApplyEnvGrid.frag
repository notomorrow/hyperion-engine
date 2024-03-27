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

#ifdef MODE_IRRADIANCE
#define MODE 0
#elif defined(MODE_RADIANCE)
#define MODE 1
#else
#define MODE 0
#endif

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

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer, size = 4352) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, size = 147456) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };

#include "./DeferredLighting.glsl"

#if MODE == 0
#include "../light_field/ComputeIrradiance.glsl"
#elif MODE == 1

HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer, size = 1310720) readonly buffer BlueNoiseBuffer
{
	ivec4 sobol_256spp_256d[256 * 256 / 4];
	ivec4 scrambling_tile[128 * 128 * 8 / 4];
	ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_SRV(Scene, VoxelGridTexture) uniform texture3D voxel_image;

#include "./EnvGridRadiance.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

void main()
{
#if MODE == 1
    // Skip every other pixel for radiance
    uvec2 screen_resolution = uvec2(camera.dimensions.xy);
    uvec2 pixel_coord = uvec2(v_texcoord * vec2(screen_resolution) - 1.0);
    const uint pixel_index = pixel_coord.y * screen_resolution.x + pixel_coord.x;

    if (bool(((pixel_coord.x & 1) ^ (pixel_coord.y & 1) ^ (scene.frame_counter & 1))))
    {
        color_output = vec4(0.0);
        return;
    }
#endif

    vec3 irradiance = vec3(0.0);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = normalize(DecodeNormal(Texture2D(sampler_nearest, gbuffer_ws_normals_texture, v_texcoord)));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);

#if MODE == 0 // Irradiance
    irradiance = CalculateEnvProbeIrradiance(P, N);

    color_output = vec4(irradiance, 1.0);
#else // Radiance
    const vec4 material = Texture2D(sampler_nearest, gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    AABB voxel_grid_aabb;
    voxel_grid_aabb.min = env_grid.voxel_grid_aabb_min.xyz;
    voxel_grid_aabb.max = env_grid.voxel_grid_aabb_max.xyz;

    vec4 radiance = ComputeVoxelRadiance(P, N, V, roughness, pixel_coord, screen_resolution, scene.frame_counter, ivec3(env_grid.density.xyz), voxel_grid_aabb);

    color_output = radiance;
#endif
}