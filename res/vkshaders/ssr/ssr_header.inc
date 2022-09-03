#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/shared.inc"

#define HYP_SSR_ROUGHNESS_MAX 1.0

layout(push_constant) uniform PushConstant
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

vec2 GetProjectedPosition(vec3 pos)
{
	vec4 samplePosition = scene.projection * vec4(pos, 1.f);
	samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
	return samplePosition.xy;
}
float isoscelesTriangleOpposite(float adjacentLength, float coneTheta)
{
    // simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj, opp = tan(theta) * adj, then multiply * 2.0f for isosceles triangle base
    return 2.0 * tan(coneTheta) * adjacentLength;
}

float isoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

vec4 coneSampleWeightedColor(vec2 samplePos, float mipChannel, float gloss)
{
    vec4 sampleColor = Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, samplePos, mipChannel);
    return sampleColor * gloss;
}

float isoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius)
{
    // subtract the diameter of the incircle to get the adjacent side of the next level on the cone
    return adjacentLength - (incircleRadius * 2.0);
}

