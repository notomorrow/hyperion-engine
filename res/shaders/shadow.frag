#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=1) in vec3 v_position;
layout(location=2) in vec2 v_texcoord0;
layout(location=7) in flat vec3 v_camera_position;
layout(location=15) in flat uint v_object_index;

layout(location=0) out vec4 output_shadow;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/object.inc"
#include "include/material.inc"
#include "include/shared.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

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

#define HYP_SHADOW_SAMPLE_ALBEDO 1

void main()
{
    // if (bool(GET_OBJECT_BUCKET(object) & OBJECT_MASK_SKY)) {
    //     discard;
    // }

#if defined(HYP_SHADOW_SAMPLE_ALBEDO) && HYP_SHADOW_SAMPLE_ALBEDO
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_texcoord0);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
            discard;
        }
    }
#endif

    const float depth = gl_FragCoord.z / gl_FragCoord.w;

#ifdef MODE_VSM
    vec2 moments = vec2(depth, HYP_FMATH_SQR(depth));

    float dx = dFdx(depth);
    float dy = dFdy(depth);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output_shadow = vec4(moments, 0.0, 0.0);
#else
    output_shadow = vec4(depth, 0.0, 0.0, 0.0);
#endif
}
