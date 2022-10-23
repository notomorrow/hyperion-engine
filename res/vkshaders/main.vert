#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=7) out flat vec3 v_camera_position;
layout(location=8) out mat3 v_tbn_matrix;
layout(location=12) out vec3 v_ndc_position;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;
layout (location = 6) in vec4 a_bone_weights;
layout (location = 7) in vec4 a_bone_indices;

#include "include/scene.inc"
#include "include/object.inc"

#define HYP_ENABLE_SKINNING 1

struct Skeleton
{
    mat4 bones[128];
};

layout(std140, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 2, row_major) readonly buffer SkeletonBuffer
{
    Skeleton skeleton;
};

mat4 CreateSkinningMatrix()
{
	mat4 skinning = mat4(0.0);

	int index0 = int(a_bone_indices.x);
	skinning += a_bone_weights.x * skeleton.bones[index0];
	int index1 = int(a_bone_indices.y);
	skinning += a_bone_weights.y * skeleton.bones[index1];
	int index2 = int(a_bone_indices.z);
	skinning += a_bone_weights.z * skeleton.bones[index2];
	int index3 = int(a_bone_indices.w);
	skinning += a_bone_weights.w * skeleton.bones[index3];

	return skinning;
}

void main() {
    vec4 position;
    mat4 normal_matrix;
    
#if HYP_ENABLE_SKINNING
    if (bool(object.skeleton_id)) {
        mat4 skinning_matrix = CreateSkinningMatrix();

        position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
        normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
    } else {
#endif
        position = object.model_matrix * vec4(a_position, 1.0);
		normal_matrix = transpose(inverse(object.model_matrix));
#if HYP_ENABLE_SKINNING
    }
#endif

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_camera_position = scene.camera_position.xyz;
    
    v_tangent = (normal_matrix * vec4(a_tangent, 0.0)).xyz;
	v_bitangent = (normal_matrix * vec4(a_bitangent, 0.0)).xyz;
	v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);
    
    vec4 ndc = scene.projection * scene.view * position;
    ndc /= ndc.w;
    v_ndc_position = ndc.xyz;

    // v_fragment_position = (scene.projection * scene.view * position).xyz;
    // v_fragment_position.xy *= vec2(0.5);
    // v_fragment_position.xy += vec2(0.5);

    gl_Position = scene.projection * scene.view * position;

} 
