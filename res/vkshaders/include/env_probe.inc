#ifndef HYP_ENV_PROBE_GLSL
#define HYP_ENV_PROBE_GLSL

#include "defines.inc"

// ENV PROBES

struct EnvProbe {
    vec4 aabb_max;
    vec4 aabb_min;
    vec4 world_position;
    uint component_index; // index of the cubemap texture
};


layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube env_probe_textures[HYP_MAX_ENV_PROBES];
layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 26) uniform EnvProbeUniforms {
    EnvProbe env_probes[HYP_MAX_ENV_PROBES];
};

vec4 SampleProbeParallaxCorrected(sampler samp, textureCube tex,
                                  EnvProbe env_probe,
                                  vec3 world, vec3 R, float lod)
{
    vec3 rbmax    = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin    = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);


    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction + vec3(0.001);
    vec3 coord = box - env_probe.world_position.xyz;

    return texture(samplerCube(tex, samp), coord, 3);
}

vec4 SampleProbeParallaxCorrected(samplerCube tex,
                                  EnvProbe env_probe,
                                  vec3 world, vec3 R, float lod)
{
    vec3 rbmax    = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin    = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);


    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction + vec3(0.001);
    vec3 coord = box - env_probe.world_position.xyz;

    return texture(tex, coord, lod);
}

vec4 SampleProbe(sampler samp, textureCube tex, vec3 R, float lod)
{
    return texture(samplerCube(tex, samp), R, lod);
}

#endif