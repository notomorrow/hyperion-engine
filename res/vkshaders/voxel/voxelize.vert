#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;
layout (location = 6) in vec4 a_bone_weights;
layout (location = 7) in vec4 a_bone_indices;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/voxel/shared.inc"

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    v_position = position.xyz;
    v_normal = (transpose(inverse(object.model_matrix)) * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
    
    vec3 aabb_max = vec3(64.0) + vec3(0.0, 0.0 , 5.0);  //object.local_aabb_max.xyz; //scene.aabb_max.xyz;  //;
    vec3 aabb_min = vec3(-64.0) + vec3(0.0, 0.0, 5.0); //object.local_aabb_min.xyz; //scene.aabb_min.xyz;  //;
    
    gl_Position = vec4(ScaleToAABB(aabb_max, aabb_min, v_position), 1.0);
}
