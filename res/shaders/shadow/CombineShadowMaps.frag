
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"

#include "../include/scene.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef STAGE_DYNAMICS
HYP_DESCRIPTOR_SRV(CombineShadowMapsDescriptorSet, PrevTexture) uniform texture2D prev_texture;
#endif

HYP_DESCRIPTOR_SRV(CombineShadowMapsDescriptorSet, InTexture) uniform texture2D src_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

void main()
{
    color_output = Texture2D(sampler_nearest, src_texture, v_texcoord);

#ifdef STAGE_DYNAMICS
    vec4 prev_color = Texture2D(sampler_nearest, prev_texture, v_texcoord);

    color_output = color_output.r < prev_color.r ? color_output : prev_color;
#endif
}
