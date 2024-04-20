#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/tonemap.inc"
#include "../include/noise.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SSBO(Global, BlueNoiseBuffer, size = 1310720) readonly buffer BlueNoiseBuffer
{
	ivec4 sobol_256spp_256d[256 * 256 / 4];
	ivec4 scrambling_tile[128 * 128 * 8 / 4];
	ivec4 ranking_tile[128 * 128 * 8 / 4];
};

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DEFERRED_NO_RT_RADIANCE
#define HYP_DEFERRED_NO_SSR
#define HYP_DEFERRED_NO_ENV_GRID

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#include "./DeferredLighting.glsl"
#include "../include/BlueNoise.glsl"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#define SAMPLE_COUNT 1 // multiple samples having issues

void main()
{
    // Skip every other pixel for radiance
    uvec2 screen_resolution = uvec2(deferred_params.screen_width, deferred_params.screen_height);
    uvec2 pixel_coord = uvec2(v_texcoord * vec2(screen_resolution - 1) + 0.5);

    if (bool(((pixel_coord.x & 1) ^ (pixel_coord.y & 1) ^ (scene.frame_counter & 1))))
    {
        color_output = vec4(0.0);
        return;
    }

    vec3 irradiance = vec3(0.0);

    const float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = normalize(DecodeNormal(Texture2D(sampler_nearest, gbuffer_normals_texture, v_texcoord)));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const vec4 material = Texture2D(sampler_nearest, gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    const float lod = float(9.0) * roughness * (2.0 - roughness);

    vec4 ibl = vec4(0.0);

    const uint probe_texture_index = max(0, min(current_env_probe.texture_index, HYP_MAX_BOUND_REFLECTION_PROBES - 1));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(N, tangent, bitangent);

    /*vec2 blue_noise_sample = vec2(
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 0),
        SampleBlueNoise(int(pixel_coord.x), int(pixel_coord.y), 0, 1)
    );

    vec2 blue_noise_scaled = blue_noise_sample + float(scene.frame_counter % 256) * 1.618;
    vec2 rnd = fmod(blue_noise_scaled, vec2(1.0));*/

#if SAMPLE_COUNT > 1
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        const vec2 rnd = Hammersley(uint(i), uint(SAMPLE_COUNT));

        vec3 H = ImportanceSampleGGX(rnd, N, roughness);
        H = tangent * H.x + bitangent * H.y + N * H.z;

        const vec3 dir = normalize(2.0 * dot(V, H) * H - V);
#else
        const vec3 dir = R;
#endif

        vec4 sample_ibl = vec4(0.0);
        ApplyReflectionProbe(current_env_probe, P, dir, lod, sample_ibl);
        ibl += sample_ibl;
#if SAMPLE_COUNT > 1
    }
#endif

    color_output = ibl * (1.0 / float(SAMPLE_COUNT));

    // color_output.rgb = TonemapReinhardSimple(color_output.rgb);
}