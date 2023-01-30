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
    float camera_near;
    float camera_far;

    uvec2 dimensions;
    uvec2 _pad2;

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
    vec3 values[9];
};

#define HYP_ENV_PROBE_PARALLAX_CORRECTED 0x1

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
    #ifndef HYP_DEFERRED_NO_ENV_GRID
        layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 2) uniform EnvGridBuffer
        {
            EnvGrid env_grid;
        };

        #define GET_GRID_PROBE_INDEX(index) (env_grid.probe_indices[index >> 2][index & 3])
        #define GET_GRID_PROBE(index) (env_probes[env_grid.probe_indices[index >> 2][index & 3]])

        layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 61) uniform texture3D spherical_harmonics_volumes[9];

    #endif

    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube env_probe_textures[HYP_MAX_BOUND_REFLECTION_PROBES];
    layout(std430, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 26, row_major) readonly buffer EnvProbesBuffer
    {
        EnvProbe env_probes[HYP_MAX_ENV_PROBES];
    };
    layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 27) uniform textureCube point_shadow_maps[HYP_MAX_BOUND_POINT_SHADOW_MAPS];
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
        sh.values[0] +

        sh.values[1] * x +
        sh.values[2] * y +
        sh.values[3] * z +

        sh.values[4] * z * x +
        sh.values[5] * y * z +
        sh.values[6] * y * x +
        sh.values[7] * (3.0 * z * z - 1.0) +
        sh.values[8] * (x * x - y * y)
    );

    result = max(result, vec3(0.0));

    return result;
}

#endif