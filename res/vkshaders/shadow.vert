#version 450
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
layout(location=12) out vec3 v_view_space_position;
layout(location=13) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec3 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#if defined(HYP_ATTRIBUTE_a_bone_weights) && defined(HYP_ATTRIBUTE_a_bone_indices)
    #define VERTEX_SKINNING_ENABLED
#endif

#include "include/scene.inc"

#define HYP_INSTANCING
#include "include/object.inc"

#ifdef VERTEX_SKINNING_ENABLED
#include "include/Skeleton.glsl"
#endif

void main()
{
    vec4 position;
    mat4 normal_matrix;
    
#ifdef VERTEX_SKINNING_ENABLED
    if (bool(object.flags & ENTITY_GPU_FLAG_HAS_SKELETON))
    {
        mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

        position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
        normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
    }
    else
#endif
    {
        position = object.model_matrix * vec4(a_position, 1.0);
		normal_matrix = transpose(inverse(object.model_matrix));
    }

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_camera_position = camera.position.xyz;
    
    v_tangent = normalize(normal_matrix * vec4(a_tangent, 0.0)).xyz;
	v_bitangent = normalize(normal_matrix * vec4(a_bitangent, 0.0)).xyz;
	v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);

    v_object_index = OBJECT_INDEX;

    gl_Position = shadow_map_data[0].projection * shadow_map_data[0].view * position;
} 
