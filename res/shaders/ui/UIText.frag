#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "../include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=4) out vec2 gbuffer_velocity;
layout(location=5) out vec4 gbuffer_mask;
layout(location=6) out vec4 gbuffer_ws_normals;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#include "../include/material.inc"
#include "../include/packing.inc"
#include "../include/env_probe.inc"
#include "../include/scene.inc"
#include "../include/object.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(UITextDescriptorSet, FontAtlasTexture) uniform texture2D font_atlas_texture;

void main()
{
    vec4 ui_color = vec4(1.0);//CURRENT_MATERIAL.albedo;
    vec4 sampled_texture = Texture2D(sampler_linear, font_atlas_texture, v_texcoord0).rrrr;

    ui_color *= sampled_texture;

    gbuffer_normals = vec4(0.0);
    gbuffer_material = vec4(0.0, 0.0, 0.0, 1.0);
    gbuffer_velocity = vec2(0.0);
    gbuffer_ws_normals = vec4(0.0);

    gbuffer_albedo = ui_color;
    gbuffer_mask = UINT_TO_VEC4(OBJECT_MASK_UI | OBJECT_MASK_UI_TEXT);
}
