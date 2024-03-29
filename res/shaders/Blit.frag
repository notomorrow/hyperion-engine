
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 out_color;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/shared.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Global, FinalOutputTexture) uniform texture2D src_image;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

void main()
{
    out_color = vec4(0.0, 0.0, 0.0, 1.0);

    // out_color.rgb = pow(Texture2D(sampler_nearest, src_image, v_texcoord0).rgb, vec3(1.0/2.2));
    out_color.rgb = Texture2D(sampler_nearest, src_image, v_texcoord0).rgb;
}
