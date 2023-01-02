#ifndef HYP_ENV_PROBE_GLSL
#define HYP_ENV_PROBE_GLSL

#include "defines.inc"

// ENV PROBES

struct EnvProbe
{
    mat4 face_view_matrices[6];

    vec4 aabb_max;
    vec4 aabb_min;
    vec4 world_position;

    uint texture_index;
    uint flags;
    uint _pad0;
    uint _pad1;

    uvec4 _pad2;
    uvec4 _pad3;
    uvec4 _pad4;
    
    uvec4 _pad5;
};

struct EnvGrid
{
    uvec4 probe_indices[HYP_MAX_BOUND_AMBIENT_PROBES / 4];

    vec4 center;
    vec4 aabb_extent;
    uvec4 density;

    uint enabled_indices_mask;
    uint _pad0;
    uint _pad1;
    uint _pad2;

    mat4 _pad3;
};

struct SH9
{
    vec4 values[16];
};

#define HYP_ENV_PROBE_PARALLAX_CORRECTED 0x1

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(set = HYP_DESCRIPTOR_SET_SCENE, binding = 2) uniform EnvGridBuffer
{
    EnvGrid env_grid;
};

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube env_probe_textures[HYP_MAX_BOUND_REFLECTION_PROBES];
layout(std430, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 26, row_major) readonly buffer EnvProbeBuffer
{
    EnvProbe env_probes[HYP_MAX_ENV_PROBES];
};

layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 60) readonly buffer SH9Buffer {
    SH9 spherical_harmonics[];
};

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 61) uniform texture3D spherical_harmonics_volumes[9];

#define GET_GRID_PROBE_INDEX(index) (env_grid.probe_indices[index >> 2][index & 3])
#define GET_GRID_PROBE(index) (env_probes[env_grid.probe_indices[index >> 2][index & 3]])

#endif

vec4 EnvProbeSample(
    sampler samp, textureCube tex,
    vec3 coord, float lod
)
{
    vec4 color = textureLod(samplerCube(tex, samp), coord, lod);
    //color.rgb = pow(color.rgb, vec3(2.2));
    return color;
}

vec3 EnvProbeCoordParallaxCorrected(
    EnvProbe env_probe,
    vec3 world, vec3 R
)
{
    vec3 rbmax = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin = (env_probe.aabb_min.xyz - world) / R;
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
    vec3 rbmax = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin = (env_probe.aabb_min.xyz - world) / R;
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
    vec3 rbmax = (env_probe.aabb_max.xyz - world) / R;
    vec3 rbmin = (env_probe.aabb_min.xyz - world) / R;
    vec3 rbminmax = max(rbmax, rbmin);

    float correction = min(min(rbminmax.x, rbminmax.y), rbminmax.z);

    vec3 box = world + R * correction;
    vec3 coord = box - env_probe.world_position.xyz;

    return textureLod(tex, coord, lod);
}

vec3 SphericalHarmonicsSample(const in SH9 sh, vec3 normal)
{
    const float x = normal.x;
    const float y = normal.y;
    const float z = normal.z;

    vec3 result = vec3(
        sh.values[0].xyz +

        sh.values[1].xyz * x +
        sh.values[2].xyz * y +
        sh.values[3].xyz * z +

        sh.values[4].xyz * z * x +
        sh.values[5].xyz * y * z +
        sh.values[6].xyz * y * x +
        sh.values[7].xyz * (3.0 * z * z - 1.0) +
        sh.values[8].xyz * (x * x - y * y)
    );

    result = max(result, vec3(0.0));

    return result;

    // normal.xz = normal.zx;
    // vec4 NormalVector = vec4(normal, 1.0);

    // // todo transpose coeffs directly
    // // NormalVector.xyz = NormalVector.zyx;

    // vec3 X0, X1, X2;
    // X0.x = dot( sh.values[0].xyz, normal) + shCoefs[0].w;
    // X0.y = dot( sh.values[1].xyz, normal) + shCoefs[1].w;
    // X0.z = dot( sh.values[2].xyz, normal) + shCoefs[2].w;

    // vec4 vB = NormalVector.zyxx * NormalVector.yxxz;
    // X1.x = dot( sh.values[3].xyz, vB.xyz) + (shCoefs[3].w * vB.w);
    // X1.y = dot( sh.values[4].xyz, vB.xyz) + (shCoefs[4].w * vB.w);
    // X1.z = dot( sh.values[5].xyz, vB.xyz) + (shCoefs[5].w * vB.w);

    // float vC = NormalVector.z * NormalVector.z - NormalVector.y * NormalVector.y;
    // X2 =  sh.values[6].xyz * vC;

    // return ( X0 + X1 + X2 );
}

#endif