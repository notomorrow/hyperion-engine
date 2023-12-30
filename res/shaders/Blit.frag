
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

#include "include/gbuffer.inc"
#include "include/shared.inc"
#include "include/PostFXSample.inc"
#include "include/rt/probe/probe_uniforms.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 80) uniform texture2D src_image;

layout(location=0) out vec4 out_color;

void main()
{
    out_color = vec4(0.0, 0.0, 0.0, 1.0);

    out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, src_image, v_texcoord0).rgb;
}
