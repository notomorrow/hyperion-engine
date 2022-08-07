
#ifndef HYP_POST_FX_SAMPLE_GLSL
#define HYP_POST_FX_SAMPLE_GLSL

#include "PostFXCommon.inc"

vec4 SampleEffectPre(uint index, vec2 texcoord, in vec4 default_value)
{
    const uint flag = 1 << index;
    const uint bitmask = post_processing.masks[HYP_STAGE_PRE] & flag;

    return bitmask != 0 ? Texture2D(effects_pre_stack[index], texcoord) : default_value;
}

vec4 SampleEffectPost(uint index, vec2 texcoord, in vec4 default_value)
{
    const uint flag = 1 << index; 
    const uint bitmask = post_processing.masks[HYP_STAGE_POST] & flag;

    return bitmask != 0 ? Texture2D(effects_post_stack[index], texcoord) : default_value;
}

vec4 SampleEffect(uint stage, uint index, vec2 texcoord, in vec4 default_value)
{
    const uint flag = 1 << index;
    const uint bitmask = mix(
        post_processing.masks[HYP_STAGE_PRE] & flag,
        post_processing.masks[HYP_STAGE_POST] & flag,
        bool(stage == post_processing.masks[HYP_STAGE_POST])
    );

    return bitmask != 0 ? Texture2D(effects_pre_stack[index], texcoord) : default_value;
}

#endif