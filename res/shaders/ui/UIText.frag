#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "../include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 out_color;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#include "../include/material.inc"
#include "../include/scene.inc"
#include "../include/object.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(UITextDescriptorSet, FontAtlasTexture) uniform texture2D font_atlas_texture;

void main()
{
    vec4 ui_color = vec4(1.0);//CURRENT_MATERIAL.albedo;
    vec4 sampled_texture = Texture2D(sampler_linear, font_atlas_texture, v_texcoord0).rrrr;

    ui_color *= sampled_texture;

    out_color = ui_color;
}
