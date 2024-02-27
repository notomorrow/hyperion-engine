#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=11) in vec4 v_position_ndc;
layout(location=12) in vec4 v_previous_position_ndc;
layout(location=15) in flat uint v_object_index;
#ifdef IMMEDIATE_MODE
layout(location=16) in vec4 v_color;
#endif

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=3) out vec4 gbuffer_tangents;
layout(location=4) out vec2 gbuffer_velocity;
layout(location=5) out vec4 gbuffer_mask;
layout(location=6) out vec4 gbuffer_ws_normals;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/material.inc"
#include "include/packing.inc"

#ifndef IMMEDIATE_MODE
#include "include/object.inc"

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

// #ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform textureCube cubemap_textures[HYP_MAX_BOUND_TEXTURES];
#endif
#else
HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];
#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures) uniform textureCube cubemap_textures[];
#endif
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

void main() {
    vec3 normal = normalize(v_normal);

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));
    
    gbuffer_albedo = vec4(0.0, 1.0, 0.0, 1.0);
    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(0.0, 0.0, 1.0, 1.0);
    gbuffer_tangents = vec4(PackNormalVec2(v_tangent), PackNormalVec2(v_bitangent));
    gbuffer_velocity = vec2(velocity);
    gbuffer_ws_normals = EncodeNormal(normal);

#ifndef IMMEDIATE_MODE
    gbuffer_mask = UINT_TO_VEC4(GET_OBJECT_BUCKET(object));
#else
    gbuffer_albedo.rgb = v_color.rgb;
    gbuffer_mask = UINT_TO_VEC4(0x400);
#endif
}
