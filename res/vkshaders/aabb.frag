#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;

#include "include/material.inc"
#include "include/packing.inc"

void main() {
    vec3 normal = normalize(v_normal);
    
    gbuffer_albedo = vec4(0.0, 1.0, 0.0, 1.0);
    gbuffer_normals = EncodeNormal(normal);
}
