#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#ifndef OUTPUT_FORMAT
    #if defined(OUTPUT_RGBA8)
        #define OUTPUT_FORMAT rgba8
        #define TEMPORAL_BLENDING_GAMMA_CORRECTION
    #elif defined(OUTPUT_RGBA16F)
        #define OUTPUT_FORMAT rgba16f
    #elif defined(OUTPUT_RGBA32F)
        #define OUTPUT_FORMAT rgba32f
    #else
        #define OUTPUT_FORMAT rgba8
        #define TEMPORAL_BLENDING_GAMMA_CORRECTION
    #endif
#endif

#define UV_FORMAT rgba16f

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/packing.inc"
#include "../include/scene.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SSR_ROUGHNESS_MAX 1.0

layout(set = 0, binding = 10) uniform texture2D gbuffer_mip_chain;
layout(set = 0, binding = 11) uniform texture2D gbuffer_normals_texture;
layout(set = 0, binding = 12) uniform texture2D gbuffer_material_texture;
layout(set = 0, binding = 13) uniform texture2D gbuffer_depth_texture;

layout(set = 0, binding = 14) uniform sampler sampler_nearest;
layout(set = 0, binding = 15) uniform sampler sampler_linear;

layout(std140, set = 0, binding = 16, row_major) readonly buffer SceneBuffer
{
    Scene scene;
};

layout(std140, set = 0, binding = 17, row_major) uniform CameraShaderData
{
	Camera camera;
};

layout(std140, set = 0, binding = 18) uniform SSRParams
{
	uvec4 dimension;
	float ray_step;
	float num_iterations;
	float max_ray_distance;
	float distance_bias;
	float offset;
	float eye_fade_start;
	float eye_fade_end;
	float screen_edge_fade_start;
	float screen_edge_fade_end;
} ssr_params;

float SampleDepth(vec2 uv)
{
	return Texture2D(sampler_nearest, gbuffer_depth_texture, uv).r;
}

/// Blue noise

layout(std140, set = 0, binding = 19) readonly buffer BlueNoiseBuffer {
	ivec4 sobol_256spp_256d[256 * 256 / 4];
	ivec4 scrambling_tile[128 * 128 * 8 / 4];
	ivec4 ranking_tile[128 * 128 * 8 / 4];
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/BlueNoise.glsl"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS