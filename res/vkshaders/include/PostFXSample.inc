
#ifndef HYP_POST_FX_SAMPLE_GLSL
#define HYP_POST_FX_SAMPLE_GLSL

#include "PostFXCommon.inc"

vec4 SampleEffectPre(uint index, vec2 texcoord, in vec4 default_value)
{
    index = clamp(index, 0, HYP_MAX_EFFECTS_PER_SLOT);

    const uint flag = 1 << index;
    const uint bitmask = post_processing.masks[HYP_STAGE_PRE] & flag;

    return bitmask != 0
        ? Texture2D(HYP_SAMPLER_NEAREST, effects_pre_stack[index], texcoord)
        : default_value;
}

vec4 SampleEffectPost(uint index, vec2 texcoord, in vec4 default_value)
{
    index = clamp(index, 0, HYP_MAX_EFFECTS_PER_SLOT);

    const uint flag = 1 << index; 
    const uint bitmask = post_processing.masks[HYP_STAGE_POST] & flag;

    return bitmask != 0
        ? Texture2D(HYP_SAMPLER_NEAREST, effects_post_stack[index], texcoord)
        : default_value;
}

vec4 SampleEffect(uint stage, uint index, vec2 texcoord, in vec4 default_value)
{
    index = clamp(index, 0, HYP_MAX_EFFECTS_PER_SLOT);

    const uint flag = 1u << index;
    const float is_post_effect = float(stage == HYP_STAGE_POST);

    const uint bitmask = mix(
        post_processing.masks[HYP_STAGE_PRE] & flag,
        post_processing.masks[HYP_STAGE_POST] & flag,
        bool(is_post_effect)
    );

    return bitmask != 0
        ? mix(
            Texture2D(HYP_SAMPLER_NEAREST, effects_pre_stack[index], texcoord),
            Texture2D(HYP_SAMPLER_NEAREST, effects_post_stack[index], texcoord),
            is_post_effect
        )
        : default_value;
}

// use from outside of a post effect to sample the last effect in the stack
vec4 SampleLastEffectInChain(uint stage, vec2 texcoord, in vec4 default_value)
{
    return post_processing.masks[stage] != 0
        ? Texture2D(HYP_SAMPLER_NEAREST, effects_post_stack[post_processing.last_enabled_indices[stage]], texcoord)
        : default_value;
}

#endif