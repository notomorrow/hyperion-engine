#ifndef HYP_SHADOWS_GLSL
#define HYP_SHADOWS_GLSL

#include "noise.inc"
#include "aabb.inc"

#define HYP_SHADOW_BIAS 0.0001
#define HYP_SHADOW_VARIABLE_BIAS 1
#define HYP_SHADOW_PENUMBRA_MIN 0.01
#define HYP_SHADOW_PENUMBRA_MAX 8.0

#define HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION 0.1

// #define HYP_SHADOW_SAMPLES_4
// #define HYP_SHADOW_SAMPLES_8
#define HYP_SHADOW_SAMPLES_16
// #define HYP_SHADOW_SAMPLES_36_TAP

// #define HYP_PENUMBRA_SAMPLES_4
#define HYP_PENUMBRA_SAMPLES_8

#define HYP_SHADOW_NOISE_SPREAD 0.0002

#define HYP_SHADOW_PENUMBRA 1
#define HYP_SHADOW_VARIANCE 1

const vec2 shadow_pcf_kernel[16] = vec2[16](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

const mat4 shadow_bias_matrix = mat4( 
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0
);

vec3 GetShadowCoord(uint index, vec3 pos)
{
    mat4 shadow_matrix = shadow_bias_matrix * shadow_map_data[index].projection * shadow_map_data[index].view;
    vec4 shadow_position = shadow_matrix * vec4(pos, 1.0);
    shadow_position.xyz /= shadow_position.w;
  
    return shadow_position.xyz;
}

float GetShadowStandard(uint index, vec3 pos, vec2 offset, float NdotL)
{
    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 shadow_sample = texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(((coord.xy + offset) * shadow_map_data[index].dimensions_scale.zw) + shadow_map_data[index].offset_uv, float(shadow_map_data[index].atlas_index)));
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.005);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowStandard(uint index, vec3 pos)
{
    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 shadow_sample = texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(coord.xy * shadow_map_data[index].dimensions_scale.zw, float(shadow_map_data[index].atlas_index)));
    const float shadow_depth = shadow_sample.r;

    const float bias = HYP_SHADOW_BIAS;

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowStandard(vec4 shadow_sample, vec3 coord, float NdotL)
{
    const float shadow_depth = shadow_sample.r;

    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    return max(step(coord.z - bias, shadow_depth), 0.0);
}

float GetShadowPCF(uint index, vec3 pos, vec2 texcoord, vec2 screen_dimensions, float NdotL)
{
#define HYP_1_OVER_16 0.0625

    // float shadow = 0.0;

    // const float radius = 0.0003;

    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[0] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[1] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[2] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[3] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[4] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[5] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[6] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[7] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[8] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[9] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[10] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[11] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[12] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[13] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[14] * radius, NdotL) * HYP_1_OVER_16;
    // shadow += GetShadowStandard(index, pos, shadow_pcf_kernel[15] * radius, NdotL) * HYP_1_OVER_16;

    // return shadow;


    AABB aabb;
    aabb.min = shadow_map_data[index].aabb_min.xyz;
    aabb.max = shadow_map_data[index].aabb_max.xyz;

    const uint atlas_index = shadow_map_data[index].atlas_index;

    const vec2 offset_uv = shadow_map_data[index].offset_uv;
    const vec2 dimensions = shadow_map_data[index].dimensions_scale.xy;
    const vec2 uv_scale = shadow_map_data[index].dimensions_scale.zw;

    if (!AABBContainsPoint(aabb, pos)) {
        return 1.0;
    }

    const vec3 coord = GetShadowCoord(index, pos);
    
    const vec2 texel_size = vec2(1.0) / dimensions;

    const vec2 a = fract(dimensions * coord.xy);
    const vec2 b = vec2(1.0) - a;

    const float shadow_map_depth = coord.z;
    const float shadow_filter_size = 0.0015;

    const float gradient_noise = InterleavedGradientNoise(texcoord * screen_dimensions - 0.5);

#if defined(HYP_SHADOW_SAMPLES_16)
    #define HYP_SHADOW_SAMPLE_COUNT 16
#elif defined(HYP_SHADOW_SAMPLES_8)
    #define HYP_SHADOW_SAMPLE_COUNT 8
#elif defined(HYP_SHADOW_SAMPLES_4)
    #define HYP_SHADOW_SAMPLE_COUNT 4
#endif

#ifdef HYP_SHADOW_SAMPLE_COUNT
    #define HYP_DEF_VOGEL_DISK(iter_index) \
        vec2 vogel_##iter_index = VogelDisk(iter_index, HYP_SHADOW_SAMPLE_COUNT, gradient_noise)

    HYP_DEF_VOGEL_DISK(0); HYP_DEF_VOGEL_DISK(1);
    HYP_DEF_VOGEL_DISK(2); HYP_DEF_VOGEL_DISK(3);

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(4); HYP_DEF_VOGEL_DISK(5);
    HYP_DEF_VOGEL_DISK(6); HYP_DEF_VOGEL_DISK(7);
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    // TODO: Precalculate vogel disk for a a set number of random values.
    HYP_DEF_VOGEL_DISK(8); HYP_DEF_VOGEL_DISK(9);
    HYP_DEF_VOGEL_DISK(10); HYP_DEF_VOGEL_DISK(11);
    HYP_DEF_VOGEL_DISK(12); HYP_DEF_VOGEL_DISK(13);
    HYP_DEF_VOGEL_DISK(14); HYP_DEF_VOGEL_DISK(15);
#endif

#undef HYP_DEF_VOGEL_DISK

    vec3 shadow_coords[HYP_SHADOW_SAMPLE_COUNT];
    vec4 shadow_samples[HYP_SHADOW_SAMPLE_COUNT];

#define HYP_FETCH_SHADOW(iter_index) \
    {                                \
        shadow_coords[iter_index] = GetShadowCoord(iter_index, pos); \
        shadow_samples[iter_index] = texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3((vogel_##iter_index * shadow_filter_size * uv_scale) + offset_uv, float(atlas_index))); \
    }

#define HYP_DO_SHADOW(iter_index) \
    { \
        shadowness += GetShadowStandard(index, pos, (vogel_##iter_index * shadow_filter_size), NdotL); \
    }

#endif
    
    float shadowness = 0.0;

#ifdef HYP_SHADOW_SAMPLES_36_TAP
    const int num_samples = 16;
    const int sqrt_num_samples = 4;
    const int radius = sqrt_num_samples - 1;

    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            vec2 weights = vec2(1.0);

            bvec2 is_first = bvec2(1.0 - step(vec2(-radius + 1), vec2(x, y)));
            bvec2 is_last = bvec2(step(vec2(radius), vec2(x, y)));

            weights = mix(weights, b, is_first);
            weights = mix(weights, a, is_last);

            const float weight = weights.x * weights.y;

            shadowness += GetShadowStandard(index, pos, (vec2(x, y) * texel_size), NdotL) * weight;
        }
    }

    shadowness /= 36.0;

#else


#if 0

    HYP_FETCH_SHADOW(0) HYP_FETCH_SHADOW(1)
    HYP_FETCH_SHADOW(2) HYP_FETCH_SHADOW(3)

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW(4) HYP_FETCH_SHADOW(5)
    HYP_FETCH_SHADOW(6) HYP_FETCH_SHADOW(7)
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_FETCH_SHADOW(8)  HYP_FETCH_SHADOW(9)
    HYP_FETCH_SHADOW(10) HYP_FETCH_SHADOW(11)
    HYP_FETCH_SHADOW(12) HYP_FETCH_SHADOW(13)
    HYP_FETCH_SHADOW(14) HYP_FETCH_SHADOW(15)
#endif

#endif

    HYP_DO_SHADOW(0) HYP_DO_SHADOW(1)
    HYP_DO_SHADOW(2) HYP_DO_SHADOW(3)

#if defined(HYP_SHADOW_SAMPLES_8) || defined(HYP_SHADOW_SAMPLES_16)
    HYP_DO_SHADOW(4) HYP_DO_SHADOW(5)
    HYP_DO_SHADOW(6) HYP_DO_SHADOW(7)
#endif

#if defined(HYP_SHADOW_SAMPLES_16)
    HYP_DO_SHADOW(8)  HYP_DO_SHADOW(9)
    HYP_DO_SHADOW(10) HYP_DO_SHADOW(11)
    HYP_DO_SHADOW(12) HYP_DO_SHADOW(13)
    HYP_DO_SHADOW(14) HYP_DO_SHADOW(15)
    
    shadowness /= 16.0;

#elif defined(HYP_SHADOW_SAMPLES_8)
    shadowness /= 8.0;
#elif defined(HYP_SHADOW_SAMPLES_4)
    shadowness /= 4.0;
#endif

#endif

#undef HYP_DO_SHADOW

    return shadowness;

#undef HYP_1_OVER_16
}

float GetShadowVariance(uint index, vec3 pos, float NdotL)
{
    float bias = HYP_SHADOW_BIAS;

#ifdef HYP_SHADOW_VARIABLE_BIAS
    bias *= tan(acos(NdotL));
    bias = clamp(bias, 0.0, 0.01);
#endif

    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 moments = texture(sampler2DArray(shadow_maps, HYP_SAMPLER_LINEAR), vec3(coord.xy * shadow_map_data[index].dimensions_scale.zw + shadow_map_data[index].offset_uv, float(index)));

    float d = coord.z - moments.x;
    float p = step(coord.z, moments.x + bias);
    float variance = max(moments.y - moments.x * moments.x, 0.0001);

    float p_max = variance / (variance + d * d);

    AABB aabb;
    aabb.min = shadow_map_data[index].aabb_min.xyz;
    aabb.max = shadow_map_data[index].aabb_max.xyz;

    return AABBContainsPoint(aabb, pos) ? max(p, p_max) : 1.0;

#if 0
    const vec3 coord = GetShadowCoord(index, pos);
    const vec4 shadow_sample = texture(sampler2DArray(shadow_maps, HYP_SAMPLER_LINEAR), vec3(coord.xy * shadow_map_data[index].dimensions_scale.zw + shadow_map_data[index].offset_uv, float(index)));
    const float moment = shadow_sample.r;

    if (coord.z <= moment) {
        return 1.0;
    }

    const float moment2 = shadow_sample.g;

    const float variance = max(moment2 - HYP_FMATH_SQR(moment), HYP_FMATH_EPSILON);
    const float d = coord.z - moment;
    
    float percent_in_shadow = variance / (variance + HYP_FMATH_SQR(d));
    percent_in_shadow = smoothstep(HYP_SHADOW_VARIANCE_LIGHT_BLEED_REDUCTION, 1.0, percent_in_shadow);

    percent_in_shadow *= 1.0 - float(NdotL > 1.5708);
    return float(acos(NdotL));

#endif
}

float AvgBlockerDepthToPenumbra(float light_size, float avg_blocker_depth, float shadow_map_coord_z)
{
    float penumbra = (shadow_map_coord_z - avg_blocker_depth) * light_size / avg_blocker_depth;
    penumbra += HYP_SHADOW_PENUMBRA_MIN;
    penumbra = min(HYP_SHADOW_PENUMBRA_MAX, penumbra);
    return penumbra;
}

float GetShadowContactHardened(uint index, vec3 pos, vec2 texcoord, vec2 screen_dimensions, float NdotL)
{
    AABB aabb;
    aabb.min = shadow_map_data[index].aabb_min.xyz;
    aabb.max = shadow_map_data[index].aabb_max.xyz;

    if (!AABBContainsPoint(aabb, pos)) {
        return 1.0;
    }

    const vec2 uv_scale = shadow_map_data[index].dimensions_scale.zw;

    const vec3 coord = GetShadowCoord(index, pos);

    const uint atlas_index = shadow_map_data[index].atlas_index;
    const vec2 offset_uv = shadow_map_data[index].offset_uv;

    const float shadow_map_depth = coord.z;

    const float shadow_filter_size = 0.001;
    const float penumbra_filter_size = 0.0005;
    const float light_size = 3.0; // affects how quickly shadows become soft

    const float gradient_noise = InterleavedGradientNoise(texcoord * screen_dimensions - 0.5);

    float total_blocker_depth = 0.0;
    float num_blockers = 0.0;


#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
    #define HYP_DEF_VOGEL_DISK(iter_index) \
        vec2 vogel_##iter_index = VogelDisk(iter_index, 16, gradient_noise)
#else
    #define HYP_DEF_VOGEL_DISK(iter_index) \
        vec2 vogel_##iter_index = VogelDisk(iter_index, 8, gradient_noise)
#endif

    HYP_DEF_VOGEL_DISK(0);
    HYP_DEF_VOGEL_DISK(2);
    HYP_DEF_VOGEL_DISK(4);
    HYP_DEF_VOGEL_DISK(6);
    HYP_DEF_VOGEL_DISK(8);
    HYP_DEF_VOGEL_DISK(10);
    HYP_DEF_VOGEL_DISK(12);
    HYP_DEF_VOGEL_DISK(14);

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_PENUMBRA_SAMPLES_16)
    HYP_DEF_VOGEL_DISK(1);
    HYP_DEF_VOGEL_DISK(3);
    HYP_DEF_VOGEL_DISK(5);
    HYP_DEF_VOGEL_DISK(7);
    HYP_DEF_VOGEL_DISK(9);
    HYP_DEF_VOGEL_DISK(11);
    HYP_DEF_VOGEL_DISK(13);
    HYP_DEF_VOGEL_DISK(15);
#endif

#undef HYP_DEF_VOGEL_DISK

#if defined(HYP_SHADOW_PENUMBRA) && HYP_SHADOW_PENUMBRA

// vectorization
#define HYP_DO_SHADOW_PENUMBRA(iter_index0, iter_index1, iter_index2, iter_index3) \
    { \
        vec4 blocker_samples = vec4( \
            texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(((coord.xy + (vogel_##iter_index0 * /*shadow_pcf_kernel[iter_index0] **/ penumbra_filter_size)) * uv_scale) + offset_uv, float(atlas_index))).r, \
            texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(((coord.xy + (vogel_##iter_index1 * /*shadow_pcf_kernel[iter_index1] **/ penumbra_filter_size)) * uv_scale) + offset_uv, float(atlas_index))).r, \
            texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(((coord.xy + (vogel_##iter_index2 * /*shadow_pcf_kernel[iter_index2] **/ penumbra_filter_size)) * uv_scale) + offset_uv, float(atlas_index))).r, \
            texture(sampler2DArray(shadow_maps, HYP_SAMPLER_NEAREST), vec3(((coord.xy + (vogel_##iter_index3 * /*shadow_pcf_kernel[iter_index3] **/ penumbra_filter_size)) * uv_scale) + offset_uv, float(atlas_index))).r \
        ); \
        vec4 are_samples_blocking = vec4(lessThan(blocker_samples, vec4(shadow_map_depth))); \
        total_blocker_depth += dot(blocker_samples * are_samples_blocking, vec4(1.0)); \
        num_blockers += dot(are_samples_blocking, vec4(1.0)); \
    }
    
#ifdef HYP_PENUMBRA_SAMPLES_8
    HYP_DO_SHADOW_PENUMBRA(0, 2, 4, 6)
    HYP_DO_SHADOW_PENUMBRA(8, 10, 12, 14)
#elif defined(HYP_PENUMBRA_SAMPLES_4)
    HYP_DO_SHADOW_PENUMBRA(0, 4, 8, 12)
#endif

#undef HYP_DO_SHADOW_PENUMBRA

    float penumbra_mask = num_blockers > 0.0 ? AvgBlockerDepthToPenumbra(light_size, total_blocker_depth / max(num_blockers, 0.0001), shadow_map_depth) : 0.0;
#else
    float penumbra_mask = 1.0;
#endif
    
    float shadowness = 0.0;

#define HYP_DO_SHADOW(iter_index) \
    { \
        shadowness += GetShadowStandard(index, pos, (vogel_##iter_index * penumbra_mask * shadow_filter_size * uv_scale), NdotL); \
    }

#if defined(HYP_SHADOW_SAMPLES_16) || defined(HYP_SHADOW_SAMPLES_8)
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(2)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(6)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(10)
    HYP_DO_SHADOW(12)
    HYP_DO_SHADOW(14)
#elif defined(HYP_SHADOW_SAMPLES_4)
    HYP_DO_SHADOW(0)
    HYP_DO_SHADOW(4)
    HYP_DO_SHADOW(8)
    HYP_DO_SHADOW(12)
#endif

#ifdef HYP_SHADOW_SAMPLES_16
    HYP_DO_SHADOW(1)
    HYP_DO_SHADOW(3)
    HYP_DO_SHADOW(5)
    HYP_DO_SHADOW(7)
    HYP_DO_SHADOW(9)
    HYP_DO_SHADOW(11)
    HYP_DO_SHADOW(13)
    HYP_DO_SHADOW(15)

    shadowness /= 16.0;
#elif defined(HYP_SHADOW_SAMPLES_8)
    shadowness /= 8.0;
#elif defined(HYP_SHADOW_SAMPLES_4)
    shadowness /= 4.0;
#endif

#undef HYP_DO_SHADOW

    return shadowness;
}

#ifdef HYP_ENV_PROBE_GLSL

float GetPointShadowVariance(uint texture_index, vec3 world_to_light)
{
    const vec2 moments = TextureCube(HYP_SAMPLER_LINEAR, point_shadow_maps[texture_index], world_to_light).rg;
    const float current_depth = length(world_to_light);

    float shadow_dist = current_depth - moments.x;
    float p = step(current_depth, moments.x);
    float variance = max(moments.y - moments.x * moments.x, 0.0001);
    float p_max = variance / (variance + shadow_dist * shadow_dist);

    return max(p, p_max);
}

float GetPointShadowStandard(uint texture_index, vec3 world_to_light)
{
    const float shadow_depth = TextureCube(HYP_SAMPLER_NEAREST, point_shadow_maps[texture_index], world_to_light).r;
    const float current_depth = length(world_to_light);

    float bias = HYP_SHADOW_BIAS;

    return max(step(current_depth - 0.001, shadow_depth), 0.0);
}

float GetPointShadow(uint texture_index, uint flags, vec3 world_to_light)
{
    switch (flags & SHADOW_FLAGS_TYPE_MASK) {
    case SHADOW_FLAGS_VSM:
        return GetPointShadowVariance(texture_index, world_to_light);
    default:
        return GetPointShadowStandard(texture_index, world_to_light);
    }
}

#endif

float GetShadow(uint index, vec3 position, vec2 texcoord, vec2 screen_dimensions, float NdotL)
{
    if (index >= HYP_MAX_SHADOW_MAPS) {
        return 1.0;
    }

    switch (shadow_map_data[index].flags & SHADOW_FLAGS_TYPE_MASK) {
    case SHADOW_FLAGS_VSM:
        return GetShadowVariance(index, position, NdotL);
    case SHADOW_FLAGS_CONTACT_HARDENED:
        return GetShadowContactHardened(index, position, texcoord, screen_dimensions, NdotL);
    case SHADOW_FLAGS_PCF:
        return GetShadowPCF(index, position, texcoord, screen_dimensions, NdotL);
    default:
        return GetShadowStandard(index, position, vec2(0.0), NdotL);
    }
}

#endif