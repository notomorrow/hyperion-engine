#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;

#include "include/material.inc"

layout(set = 6, binding = 0) uniform samplerCube textures[];

void main() {
    vec3 normal = normalize(v_normal);
    
    gbuffer_albedo = vec4(0.0); /* for now until we have
        an attachment for txcoord / material idx
        that can show the deferred renderer we
        don't want env mapping on this */   //vec4(textureLod(textures[material.texture_index[0]], v_position, 7.0).rgb, 1.0);
    
    gbuffer_normals = vec4(normal * 0.5 + 0.5, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}