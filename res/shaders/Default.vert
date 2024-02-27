#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=3) out vec2 v_texcoord1;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=7) out flat vec3 v_camera_position;
layout(location=8) out mat3 v_tbn_matrix;
layout(location=11) out vec4 v_position_ndc;
layout(location=12) out vec4 v_previous_position_ndc;
layout(location=15) out flat uint v_object_index;
layout(location=16) out flat uint v_object_mask;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#if defined(HYP_ATTRIBUTE_a_bone_weights) && defined(HYP_ATTRIBUTE_a_bone_indices)
    #define VERTEX_SKINNING_ENABLED
#endif

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/scene.inc"

#define HYP_INSTANCING
#include "include/object.inc"

#ifdef VERTEX_SKINNING_ENABLED
#include "include/Skeleton.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, EntityInstanceBatchesBuffer, size = 256) readonly buffer EntityInstanceBatchesBuffer
{
    EntityInstanceBatch entity_instance_batch;
};

#ifdef VERTEX_SKINNING_ENABLED
HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, SkeletonsBuffer, size = 16384) readonly buffer SkeletonsBuffer
{
    Skeleton skeleton;
};

mat4 CreateSkinningMatrix(ivec4 bone_indices, vec4 bone_weights)
{
	mat4 skinning = mat4(0.0);

	int index0 = min(bone_indices.x, HYP_MAX_BONES - 1);
	skinning += bone_weights.x * skeleton.bones[index0];
	int index1 = min(bone_indices.y, HYP_MAX_BONES - 1);
	skinning += bone_weights.y * skeleton.bones[index1];
	int index2 = min(bone_indices.z, HYP_MAX_BONES - 1);
	skinning += bone_weights.z * skeleton.bones[index2];
	int index3 = min(bone_indices.w, HYP_MAX_BONES - 1);
	skinning += bone_weights.w * skeleton.bones[index3];

	return skinning;
}
#endif

void main() {
    vec4 position;
    vec4 previous_position;
    mat4 normal_matrix;

#ifdef VERTEX_SKINNING_ENABLED
    if (bool(object.flags & ENTITY_GPU_FLAG_HAS_SKELETON))
    {
        mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

        position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
        previous_position = object.previous_model_matrix * skinning_matrix * vec4(a_position, 1.0);
        normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
    }
    else
#endif
    {
        position = object.model_matrix * vec4(a_position, 1.0);
        previous_position = object.previous_model_matrix * vec4(a_position, 1.0);
		normal_matrix = transpose(inverse(object.model_matrix));
    }

    v_position = position.xyz / position.w;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_texcoord1 = a_texcoord1.xy;
    v_camera_position = camera.position.xyz;
    v_tangent = a_tangent;//(normal_matrix * vec4(a_tangent, 0.0)).xyz;
	v_bitangent = (normal_matrix * vec4(a_bitangent, 0.0)).xyz;
	v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);

    mat4 jitter_matrix = mat4(1.0);
    jitter_matrix[3][0] += camera.jitter.x;
    jitter_matrix[3][1] += camera.jitter.y;

    v_position_ndc = (jitter_matrix * camera.projection) * camera.view * position;
    v_previous_position_ndc = (jitter_matrix * camera.projection) * camera.previous_view * previous_position;

    v_object_index = OBJECT_INDEX;

    const uint bucket = object.bucket;
    v_object_mask = (uint(bucket == HYP_OBJECT_BUCKET_OPAQUE) * OBJECT_MASK_OPAQUE)
        | (uint(bucket == HYP_OBJECT_BUCKET_TRANSLUCENT) * OBJECT_MASK_TRANSLUCENT)
        | (uint(bucket == HYP_OBJECT_BUCKET_SKYBOX) * OBJECT_MASK_SKY)
        | (uint(bucket == HYP_OBJECT_BUCKET_UI) * OBJECT_MASK_UI);

    gl_Position = v_position_ndc;
} 
