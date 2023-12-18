#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#ifdef MODE_IRRADIANCE
#define MODE 0
#elif defined(MODE_RADIANCE)
#define MODE 1
#else
#define MODE 0
#endif

#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
#define HYP_DEFERRED_NO_SSR // temp

#include "./DeferredLighting.glsl"
#include "../include/env_probe.inc"

// #define USE_TEXTURE_ARRAY

#ifdef USE_TEXTURE_ARRAY
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 63) uniform texture2DArray light_field_color_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 64) uniform texture2DArray light_field_normals_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 65) uniform texture2DArray light_field_depth_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 66) uniform texture2DArray light_field_irradiance_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 67) uniform texture2DArray light_field_depth_buffer_lowres;
#else
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 63) uniform texture2D light_field_color_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 64) uniform texture2D light_field_normals_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 65) uniform texture2D light_field_depth_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 66) uniform texture2D light_field_irradiance_buffer;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 67) uniform texture2D light_field_depth_buffer_lowres;
#endif

#if MODE == 0
#include "../light_field/ComputeIrradiance.glsl"
#elif MODE == 1
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 68) uniform texture3D voxel_image;

#include "./EnvGridRadiance.glsl"
#endif

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

int CubeRoot(int x)
{
    return int(round(pow(float(x), 1.0 / 3.0)));
}

#define USE_CLIPMAP

void main()
{
    vec3 irradiance = vec3(0.0);

    const float depth = SampleGBuffer(gbuffer_depth_texture, v_texcoord).r;
    const vec3 N = normalize(DecodeNormal(SampleGBuffer(gbuffer_ws_normals_texture, v_texcoord)));
    const vec3 P = SampleGBuffer(gbuffer_ws_positions_texture, v_texcoord).xyz;
    const vec3 V = normalize(camera.position.xyz - P);

#if MODE == 0

#if 0

#ifdef USE_CLIPMAP
    // Get probe cage size using textureSize of the clipmap texturearray.
    // The actual 3d size will be the height, cubed.
    const ivec2 image_dimensions = textureSize(sampler2D(sampler_linear, sh_clipmaps)).xy;
    const int product = image_dimensions.x * image_dimensions.y;
    const int cube_root = CubeRoot(product);
    const ivec3 cage_size = ivec3(cube_root, cube_root, cube_root);

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
        irradiance += bands[i] * texture(sampler2D(sampler_linear, sh_clipmaps), vec3(cage_coord.xy, float(i))).rgb;
    }

    irradiance = max(irradiance, vec3(0.0));
    irradiance /= HYP_FMATH_PI;

    color_output = vec4(irradiance, 1.0);
#else
    float weight = CalculateEnvProbeIrradiance(P, N, irradiance);

    color_output = vec4(irradiance, 1.0);
#endif

#else
    irradiance = ComputeLightFieldProbeIrradiance(P, N, V, env_grid.center.xyz, env_grid.aabb_extent.xyz, ivec3(env_grid.density.xyz));

    color_output = vec4(irradiance, 1.0);
#endif

#else // Radiance
    const vec4 material = SampleGBuffer(gbuffer_material_texture, v_texcoord); 
    const float roughness = material.r;

    vec4 radiance = ComputeVoxelRadiance(P, N, V, roughness, env_grid.center.xyz, env_grid.aabb_extent.xyz, ivec3(env_grid.density.xyz));

    color_output = radiance;
#endif
}