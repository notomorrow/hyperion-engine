#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout     : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

#include "include/scene.inc"

void main()
{
    mat4 v = scene.view;
    mat4 p = scene.projection;
    mat4 vp = p * v;
    mat4 v2 = shadow_data.matrices[0].view;
    mat4 p2 = shadow_data.matrices[0].projection;
    mat4 vp2 = p2 * v2;
}