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

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp

#include "./DeferredLighting.glsl"
#include "../include/env_probe.inc"

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

    float weight = CalculateEnvProbeIrradiance(P, N, irradiance);

    color_output = vec4(irradiance, weight);
}