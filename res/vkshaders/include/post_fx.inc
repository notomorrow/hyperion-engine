#ifndef HYP_POST_FX_GLSL
#define HYP_POST_FX_GLSL

#include "shared.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 8) uniform sampler2D effects_pre_stack[];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 9) uniform sampler2D effects_post_stack[];

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 10) uniform PostProcessingData {
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
} post_processing;

#define HYP_STAGE_PRE  0
#define HYP_STAGE_POST 1

vec4 SampleEffectPre(uint index, vec2 texcoord, in vec4 default_value)
{
    uint flag = 1 << index;
    
    uint bitmask = post_processing.masks[HYP_STAGE_PRE] & flag;

    return bitmask != 0 ? Texture2D(effects_pre_stack[index], texcoord) : default_value;
}

vec4 SampleEffectPost(uint index, vec2 texcoord, in vec4 default_value)
{
    uint flag = 1 << index;
    
    uint bitmask = post_processing.masks[HYP_STAGE_POST] & flag;

    return bitmask != 0 ? Texture2D(effects_post_stack[index], texcoord) : default_value;
}

vec4 SampleEffect(uint stage, uint index, vec2 texcoord, in vec4 default_value)
{
    uint flag = 1 << index;
    uint bitmask = 0;

    if (stage == HYP_STAGE_PRE) {
        bitmask = post_processing.masks[HYP_STAGE_PRE] & flag;

        return bitmask != 0 ? Texture2D(effects_pre_stack[index], texcoord) : default_value;
    } else {
        bitmask = post_processing.masks[HYP_STAGE_POST] & flag;

        return bitmask != 0 ? Texture2D(effects_post_stack[index], texcoord) : default_value;
    }
}

#endif