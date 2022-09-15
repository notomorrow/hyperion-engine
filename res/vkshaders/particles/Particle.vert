#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=3) out vec4 v_color;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/object.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Particle.glsl"

layout(std140, set = 0, binding = 4, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

void main()
{
    const int instance_id = gl_InstanceIndex;

    vec4 position;
    mat4 normal_matrix;
    
    // position = object.model_matrix * vec4(a_position, 1.0);
    // normal_matrix = transpose(inverse(object.model_matrix));

    position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_normal = a_normal;
    // v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);

    ParticleShaderData instance = instances[instance_id];
    position.xyz += instance.position.xyz;

    mat4 view_matrix = scene.view;
    view_matrix[0].xyz = vec3(1.0, 0.0, 0.0);
    // view_matrix[1].xyz = vec3(0.0, 1.0, 1.0);
    view_matrix[2].xyz = vec3(0.0, 0.0, 1.0);

    v_color = unpackUnorm4x8(instance.color_packed);

    gl_Position = scene.projection * view_matrix * position;
} 
