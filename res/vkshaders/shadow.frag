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

#include "include/object.inc"
#include "include/material.inc"
#include "include/shared.inc"

#define HYP_SHADOW_SAMPLE_ALBEDO 1
#define HYP_SHADOW_VARIANCE 1

void main()
{
    if (bool(GET_OBJECT_BUCKET(object) & OBJECT_MASK_SKY)) {
        discard;
    }

#if defined(HYP_SHADOW_SAMPLE_ALBEDO) && HYP_SHADOW_SAMPLE_ALBEDO
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_texcoord0);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
            discard;
        }
    }
#endif

    const float depth = gl_FragCoord.z / gl_FragCoord.w;

#if defined(HYP_SHADOW_VARIANCE) && HYP_SHADOW_VARIANCE
    vec2 moments = vec2(depth, HYP_FMATH_SQR(depth));

    float dx = dFdx(depth);
    float dy = dFdy(depth);

    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output_shadow = vec4(moments, 0.0, 0.0);
#else
    output_shadow = vec4(depth, 0.0, 0.0, 0.0);
#endif
}
