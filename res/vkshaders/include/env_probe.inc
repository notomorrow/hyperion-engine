#ifndef HYP_ENV_PROBE_GLSL
#define HYP_ENV_PROBE_GLSL

#include "defines.inc"

// ENV PROBES

struct EnvProbe {
    vec4 aabb_max;
    vec4 aabb_min;
    vec4 world_position;
    uint texture_index;
    uint flags;
};

#define HYP_ENV_PROBE_PARALLAX_CORRECTED 0x1

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube env_probe_textures[HYP_MAX_BOUND_ENV_PROBES];
layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 26) uniform EnvProbeUniforms {
    EnvProbe env_probes[HYP_MAX_ENV_PROBES];
};

vec4 EnvProbeSample(
    sampler samp, textureCube tex,
    vec3 coord, float lod
)
{
    return textureLod(samplerCube(tex, samp), coord, lod);
}

vec3 EnvProbeCoordParallaxCorrected(
    EnvProbe env_probe,
    vec3 world, vec3 R
)
{
    vec3 rbmax    = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin    = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction;
    return box - env_probe.world_position.xyz;
}

vec4 EnvProbeSampleParallaxCorrected(
    sampler samp, textureCube tex,
    EnvProbe env_probe,
    vec3 world, vec3 R, float lod
)
{
    vec3 rbmax    = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin    = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction;
    vec3 coord = box - env_probe.world_position.xyz;

    return textureLod(samplerCube(tex, samp), coord, lod);
}

vec4 EnvProbeSampleParallaxCorrected(
    samplerCube tex,
    EnvProbe env_probe,
    vec3 world, vec3 R, float lod
)
{
    vec3 rbmax    = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin    = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction;
    vec3 coord = box - env_probe.world_position.xyz;

    return textureLod(tex, coord, lod);
}

#endif