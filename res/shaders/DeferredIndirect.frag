#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, SSAOResultTexture) uniform texture2D ssao_gi_result;
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;
HYP_DESCRIPTOR_SRV(Global, EnvGridRadianceResultTexture) uniform texture2D env_grid_radiance_texture;
HYP_DESCRIPTOR_SRV(Global, EnvGridIrradianceResultTexture) uniform texture2D env_grid_irradiance_texture;
HYP_DESCRIPTOR_SRV(Global, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

#include "include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[]; };
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe) readonly buffer CurrentEnvProbe { EnvProbe current_env_probe; };

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "include/gbuffer.inc"
#include "include/material.inc"

#include "include/scene.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer) readonly buffer ScenesBuffer
{
    Scene scene;
};

#include "include/PhysicalCamera.inc"

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

#include "include/rt/probe/probe_uniforms.inc"

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, size = 256) uniform DDGIUniformBuffer
{
    DDGIUniforms probe_system;
};

HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture) uniform texture2D probe_irradiance;
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture) uniform texture2D probe_depth;
#include "include/DDGI.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#define DDGI_MULTIPLIER 1.5

void main()
{
    vec4 albedo = Texture2D(sampler_nearest, gbuffer_albedo_texture, texcoord);
    uint mask = VEC4_TO_UINT(Texture2D(sampler_nearest, gbuffer_mask_texture, texcoord));
    vec3 normal = DecodeNormal(Texture2D(sampler_nearest, gbuffer_normals_texture, texcoord));
    vec3 ws_normal = DecodeNormal(Texture2D(sampler_nearest, gbuffer_ws_normals_texture, texcoord));

    float depth = Texture2D(sampler_nearest, gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);
    vec4 material = Texture2D(sampler_nearest, gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    vec3 result = vec3(0.0);

    vec3 N = normalize(normal);
    vec3 V = normalize(camera.position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    
    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);
    vec3 F = vec3(0.0);

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_NEAREST, ssao_gi_result, v_texcoord0);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);

    const float roughness = material.r;
    const float metalness = material.g;

    float NdotV = max(0.0001, dot(N, V));

    const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
    const vec3 F0 = CalculateF0(albedo.rgb, metalness);

    F = CalculateFresnelTerm(F0, roughness, NdotV);
    const vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

    const float perceptual_roughness = sqrt(roughness);
    const vec3 dfg = CalculateDFG(F, roughness, NdotV);
    const vec3 E = CalculateE(F0, dfg);
    const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

    reflections = Texture2D(HYP_SAMPLER_NEAREST, reflections_texture, texcoord);

    vec4 env_grid_radiance = Texture2D(HYP_SAMPLER_NEAREST, env_grid_radiance_texture, texcoord);
    reflections = reflections * (1.0 - env_grid_radiance.a) + (vec4(env_grid_radiance.rgb, 1.0) * env_grid_radiance.a);

    irradiance += Texture2D(HYP_SAMPLER_NEAREST, env_grid_irradiance_texture, texcoord).rgb * ENV_GRID_MULTIPLIER;

#ifdef RT_REFLECTIONS_ENABLED
    CalculateRaytracingReflection(deferred_params, texcoord, reflections);
#endif

#ifdef RT_GI_ENABLED
    irradiance += DDGISampleIrradiance(position.xyz, ws_normal, V).rgb * DDGI_MULTIPLIER;
#endif

#ifdef HBIL_ENABLED
    CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);
#endif

    vec3 Fd = diffuse_color.rgb * irradiance * (1.0 - E) * ao;

    vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
    specular_ao *= energy_compensation;

    vec3 Fr = ibl * E * specular_ao;

    vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
    Fd *= multibounce;
    Fr *= multibounce;

    reflections.rgb *= specular_ao;
    Fr = Fr * (1.0 - reflections.a) + (E * reflections.rgb);

    result = Fd + Fr;

#ifdef PATHTRACER
    result = CalculatePathTracing(deferred_params, texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
    result = E * reflections.rgb;
#elif defined(DEBUG_IRRADIANCE)
    result = irradiance.rgb;
#endif

    output_color = vec4(result, 1.0);
}