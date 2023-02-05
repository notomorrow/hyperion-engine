#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord;
layout(location=0) out vec4 color_output;

#include "../include/defines.inc"
#include "../include/shared.inc"

layout(set = 0, binding = 0) uniform texture2D input_texture;
layout(set = 0, binding = 1) uniform sampler sampler_linear;
layout(set = 0, binding = 2) uniform sampler sampler_nearest;

layout(push_constant) uniform PushConstant
{
    uvec4 mip_dimensions;
    uvec4 previous_mip_dimensions;
    uint mip_level
};

void main()
{
    vec4 input_color = Texture2D(sampler_linear, input_texture, v_texcoord);

    color_output = input_color;
}