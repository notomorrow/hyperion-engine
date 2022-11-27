#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) out vec3 v_position;
layout (location = 1) out vec3 v_screen_space_position;
layout (location = 2) out vec2 v_texcoord0;
layout (location = 3) out flat uint v_object_index;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

#include "../include/scene.inc"

#define HYP_INSTANCING
#include "../include/object.inc"

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    vec4 ndc_position = scene.projection * scene.view * position;

    v_position = position.xyz;
    v_screen_space_position = vec3(ndc_position.xy * 0.5 + 0.5, ndc_position.z);
    v_texcoord0 = a_texcoord0;

    v_object_index = OBJECT_INDEX;

    gl_Position = ndc_position;

} 