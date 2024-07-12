#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_screen_space_position;
layout(location=2) in vec2 v_texcoord0;
layout(location=3) in flat uint v_object_index;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=5) out vec4 gbuffer_mask;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/object.inc"
#include "../include/UIObject.glsl"

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

// #ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];
#endif

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
// #else

// HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
// {
//     Material material;
// };

// #ifndef CURRENT_MATERIAL
//     #define CURRENT_MATERIAL material
// #endif
// #endif

void main()
{
    vec4 tex = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_UI_CACHE, v_texcoord0);
    ui_color = tex;
    
    uint mask = GET_OBJECT_BUCKET(object);

    gbuffer_albedo = ui_color;
    gbuffer_mask = UINT_TO_VEC4(mask);
}
