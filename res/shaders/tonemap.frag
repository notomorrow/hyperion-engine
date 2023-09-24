#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"
#include "include/PostFXInstance.inc"
#include "include/tonemap.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=0) out vec4 color_output;

void main()
{
    vec4 color_input = SamplePrevEffectInChain(v_texcoord0, vec4(0.0));

    color_output = vec4(Tonemap(color_input.rgb), 1.0);
}