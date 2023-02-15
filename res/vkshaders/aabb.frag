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

#include "include/material.inc"
#include "include/packing.inc"

#ifndef IMMEDIATE_MODE
#include "include/object.inc"
#endif

void main() {
    vec3 normal = normalize(v_normal);

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));
    
    gbuffer_albedo = vec4(0.0, 1.0, 0.0, 1.0);
    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(0.0, 0.0, 1.0, 1.0);
    gbuffer_tangents = vec4(PackNormalVec2(v_tangent), PackNormalVec2(v_bitangent));
    gbuffer_velocity = vec2(velocity);

#ifndef IMMEDIATE_MODE
    gbuffer_mask = UINT_TO_VEC4(GET_OBJECT_BUCKET(object));
#else
    gbuffer_albedo.rgb = v_color.rgb;
    gbuffer_mask = UINT_TO_VEC4(0x400);
#endif
}
