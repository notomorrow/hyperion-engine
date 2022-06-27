#ifndef HYP_POST_FX_GLSL
#define HYP_POST_FX_GLSL

#include "shared.inc"
#include "gbuffer.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 8) uniform sampler2D effects_pre_stack[];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 9) uniform sampler2D effects_post_stack[];

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 10) uniform PostProcessingData {
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
} post_processing;

layout(push_constant) uniform PushConstant {
    uint current_effect_index_stage; // 31 bits for index, 1 bit for stage
} this_effect;

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

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(uint stage, uint index, vec2 texcoord, in vec4 default_value)
{
    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return Texture2D(gbuffer_deferred_result, texcoord);
        }
    } else {
        return Texture2D(effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(uint index_stage_combined, vec2 texcoord, in vec4 default_value)
{
    uint index = index_stage_combined >> 1;
    uint stage = index_stage_combined & 0x1;

    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return Texture2D(gbuffer_deferred_result, texcoord);
        }
    } else {
        return Texture2D(effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}

// use from post fx, to sample the image "behind" this
vec4 SamplePrevEffectInChain(vec2 texcoord, in vec4 default_value)
{
    uint index = this_effect.current_effect_index_stage >> 1;
    uint stage = this_effect.current_effect_index_stage & 0x1;

    if (index == 0) {
        if (stage == HYP_STAGE_POST) {
            return Texture2D(gbuffer_deferred_result, texcoord);
        }
    } else {
        return Texture2D(effects_post_stack[index - 1], texcoord);
    }

    return default_value;
}


#endif