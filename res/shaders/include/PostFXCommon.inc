#ifndef HYP_POST_FX_COMMON_GLSL
#define HYP_POST_FX_COMMON_GLSL

#include "shared.inc"
#include "gbuffer.inc"

#define HYP_MAX_EFFECTS_PER_SLOT 4

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 8) uniform texture2D effects_pre_stack[HYP_MAX_EFFECTS_PER_SLOT];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 9) uniform texture2D effects_post_stack[HYP_MAX_EFFECTS_PER_SLOT];

layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 10) uniform PostProcessingData {
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
} post_processing;

#define HYP_STAGE_PRE  0
#define HYP_STAGE_POST 1

#endif