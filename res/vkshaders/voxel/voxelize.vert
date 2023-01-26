#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=3) out vec3 v_voxel;
layout(location=4) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_INSTANCING
#include "../include/object.inc"

#define HYP_VCT_MODE HYP_VCT_MODE_SVO
#include "../include/vct/shared.inc"

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    v_position = position.xyz;
    v_normal = (transpose(inverse(object.model_matrix)) * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
    
    // vec3 aabb_max = vec3(15.0) + vec3(0.0, 0.0, 0.0);  //object.local_aabb_max.xyz; //scene.aabb_max.xyz;  //;
    // vec3 aabb_min = vec3(-15.0) + vec3(0.0, 0.0, 0.0); //object.local_aabb_min.xyz; //scene.aabb_min.xyz;  //;

    v_voxel = VctWorldToAABB(position.xyz);

    v_object_index = OBJECT_INDEX;
    
    gl_Position = vec4(v_voxel, 1.0);
}
