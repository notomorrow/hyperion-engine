#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/shared.inc"

#define HYP_SSR_ROUGHNESS_MAX      0.5

layout(push_constant) uniform PushConstant
{
	ivec2 dimension;
	float ray_step;
	float num_iterations;
	float max_ray_distance;
} ssr_params;

vec3 generatePositionFromDepth(vec2 texturePos, float depth)
{
	vec4 ndc = vec4((texturePos - 0.5) * 2, depth, 1.f);
	vec4 inversed = inverse(scene.projection) * ndc;// going back from projected
	inversed /= inversed.w;
	return inversed.xyz;
}

vec2 generateProjectedPosition(vec3 pos)
{
	vec4 samplePosition = scene.projection * vec4(pos, 1.f);
	samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
	return samplePosition.xy;
}