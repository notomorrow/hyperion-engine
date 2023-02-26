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

#define USE_CLIPMAP

void main()
{
    vec3 irradiance = vec3(0.0);

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = DecodeNormal(SampleGBuffer(gbuffer_normals_texture, v_texcoord));
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), v_texcoord, depth).xyz;

#ifdef USE_CLIPMAP
    #define PROBE_CAGE_VIEW_RANGE 150.0

    const ivec3 cage_size = textureSize(sampler3D(sh_clipmaps[0], sampler_linear), 0);

    const vec3 scale = vec3(PROBE_CAGE_VIEW_RANGE) / vec3(cage_size.xyz);
    const vec3 camera_position_snapped = (floor(camera.position.xyz / scale) + 0.5) * scale;

    vec3 relative_position = P - camera_position_snapped;

    vec3 cage_size_world = vec3(cage_size) * PROBE_CAGE_VIEW_RANGE;
    vec3 cage_coord = (relative_position / PROBE_CAGE_VIEW_RANGE) + 0.5;

    const float cos_a0 = HYP_FMATH_PI;
    const float cos_a1 = (2.0 * HYP_FMATH_PI) / 3.0;
    const float cos_a2 = HYP_FMATH_PI * 0.25;

    float bands[9] = ProjectSHBands(N);
    bands[0] *= cos_a0;
    bands[1] *= cos_a1;
    bands[2] *= cos_a1;
    bands[3] *= cos_a1;
    bands[4] *= cos_a2;
    bands[5] *= cos_a2;
    bands[6] *= cos_a2;
    bands[7] *= cos_a2;
    bands[8] *= cos_a2;

    irradiance = vec3(0.0);

    for (int i = 0; i < 9; i++) {
        irradiance += Texture3D(sampler_linear, sh_clipmaps[i], cage_coord).rgb * bands[i];
    }

    irradiance = max(irradiance, vec3(0.0));
    irradiance /= HYP_FMATH_PI;

    color_output = vec4(irradiance, 1.0);
#else
    float weight = CalculateEnvProbeIrradiance(P, N, irradiance);

    color_output = vec4(irradiance, weight);
#endif
}