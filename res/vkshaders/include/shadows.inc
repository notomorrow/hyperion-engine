#ifndef HYP_SHADOWS_GLSL
#define HYP_SHADOWS_GLSL

#include "noise.inc"

#define HYP_SHADOW_BIAS 0.005
#define HYP_SHADOW_VARIABLE_BIAS 1
#define HYP_SHADOW_PENUMBRA 1
#define HYP_SHADOW_NUM_SAMPLES 16
#define HYP_SHADOW_PENUMBRA_NUM_SAMPLES 16
#define HYP_SHADOW_PENUMBRA_MIN 0.1
#define HYP_SHADOW_PENUMBRA_MAX 8.0

const mat4 shadow_bias_matrix = mat4( 
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

vec3 GetShadowCoord(uint index, vec3 pos)
{
    mat4 shadow_matrix = shadow_bias_matrix * shadow_data.matrices[index].projection * shadow_data.matrices[index].view;
    vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
    shadow_position.xyz /= shadow_position.w;
  
    return shadow_position.xyz;
}

float GetShadow(uint index, vec3 pos, vec2 offset, float NdotL)
{
    vec3 coord = GetShadowCoord(index, pos);
    vec4 shadow_sample = texture(shadow_maps[index], coord.xy + offset);
    float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#if HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(uint index, vec3 pos, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.0075;
    const float penumbra_filter_size = 0.015;
    const float light_size = 1.0; // affects how quickly shadows become soft

    const float gradient_noise = InterleavedGradientNoise(texcoord * vec2(uvec2(scene.resolution_x, scene.resolution_y)));

    float total_blocker_depth = 0.0;
    float num_blockers = 0.0;

    for (int i = 0; i < HYP_SHADOW_PENUMBRA_NUM_SAMPLES; i++) {
        vec2 filter_uv = VogelDisk(i, HYP_SHADOW_PENUMBRA_NUM_SAMPLES, gradient_noise);
        float blocker_sample = texture(shadow_maps[index], coord.xy + (filter_uv * penumbra_filter_size)).r;

        float is_blocking = float(blocker_sample < shadow_map_depth);

        total_blocker_depth += blocker_sample * is_blocking;
        num_blockers        += is_blocking;
    }
    
    float penumbra_mask = num_blockers > 0.0 ? AvgBlockerDepthToPenumbra(light_size, total_blocker_depth / num_blockers, shadow_map_depth) : 0.0;
    float shadowness = 0.0;

    for (int i = 0; i < HYP_SHADOW_NUM_SAMPLES; i++) {
        vec2 filter_uv = VogelDisk(i, HYP_SHADOW_NUM_SAMPLES, gradient_noise);
        shadowness += GetShadow(index, pos, filter_uv * penumbra_mask * shadow_filter_size, NdotL);
    }

    shadowness /= float(HYP_SHADOW_NUM_SAMPLES);

    return shadowness;
}
#endif