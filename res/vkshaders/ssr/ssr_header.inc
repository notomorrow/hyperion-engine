#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/packing.inc"
#include "../include/scene.inc"
#include "../include/shared.inc"
#include "../include/noise.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define HYP_SSR_ROUGHNESS_MAX 1.0

layout(set = 0, binding = 10) uniform texture2D gbuffer_albedo_texture;
layout(set = 0, binding = 11) uniform texture2D gbuffer_normals_texture;
layout(set = 0, binding = 12) uniform texture2D gbuffer_material_texture;
layout(set = 0, binding = 13) uniform texture2D gbuffer_depth_texture;

layout(set = 0, binding = 14) uniform sampler sampler_nearest;
layout(set = 0, binding = 15) uniform sampler sampler_linear;

layout(std140, set = 0, binding = 16, row_major) readonly buffer SceneBuffer
{
    Scene scene;
};

layout(std140, set = 0, binding = 17) uniform SSRParams
{
	ivec2 dimension;
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