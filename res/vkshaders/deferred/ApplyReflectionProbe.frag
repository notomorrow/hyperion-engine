#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/tonemap.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE
#define HYP_DEFERRED_NO_SSR
#define HYP_DEFERRED_NO_ENV_GRID

#include "../include/env_probe.inc"
#include "./DeferredLighting.glsl"

layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 3) readonly buffer EnvProbeBuffer
{
    EnvProbe current_env_probe;
};

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

void main()
{
    vec3 irradiance = vec3(0.0);

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = DecodeNormal(SampleGBuffer(gbuffer_normals_texture, v_texcoord));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), v_texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const vec4 material = SampleGBuffer(gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;
    const float perceptual_roughness = sqrt(roughness);

    vec4 ibl = CalculateReflectionProbe(current_env_probe, P, N, R, camera.position.xyz, perceptual_roughness);

    color_output = ibl;
}