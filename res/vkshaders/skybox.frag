#version 450
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier    : require
#extension GL_EXT_scalar_block_layout     : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;

#include "include/material.inc"
#include "include/packing.inc"

void main() {
    vec3 normal = normalize(v_normal);
    
    gbuffer_albedo    = vec4(0.0);
    //gbuffer_albedo    = vec4(textureLod(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), v_position, 0.0).rgb, 0.0 /* just for now to tell deferred to not perform lighting */);
    gbuffer_normals   = EncodeNormal(normal);
    gbuffer_positions = vec4(v_position, 1.0);
}
