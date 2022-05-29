#ifndef HYP_POST_FX_GLSL
#define HYP_POST_FX_GLSL

layout(set = 1, binding = 8) uniform sampler2D effects_pre_stack[];
layout(set = 1, binding = 9) uniform sampler2D effects_post_stack[];

layout(set = 1, binding = 10) uniform PostProcessingData {
    uint num_pre_effects;
    uint last_enabled_pre_effect_index;
    uint num_post_effects;
    uint last_enabled_post_effect_index;
    uint pre_effect_enabled_mask;
    uint post_effect_enabled_mask;
} post_processing;

#endif