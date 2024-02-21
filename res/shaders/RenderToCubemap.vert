#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_multiview : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=7) out flat vec3 v_camera_position;
layout(location=8) out mat3 v_tbn_matrix;
layout(location=11) out flat uint v_object_index;
layout(location=12) out flat vec3 v_env_probe_extent;
layout(location=13) out flat uint v_cube_face_index;
layout(location=14) out vec2 v_cube_face_uv;
layout(location=15) out vec4 v_view_space_position;

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
#include "include/env_probe.inc"

#define HYP_ENABLE_SKINNING

#ifdef VERTEX_SKINNING_ENABLED
#include "include/Skeleton.glsl"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CameraShaderData
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
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

void main() 
{
    vec4 position;
    mat4 normal_matrix;

    // if (object.bucket == HYP_OBJECT_BUCKET_SKYBOX) {
    //     position = vec4((a_position * 150.0) + camera.position.xyz, 1.0);
    //     normal_matrix = transpose(inverse(object.model_matrix));
    // } else {
#ifdef VERTEX_SKINNING_ENABLED
        if (bool(object.flags & ENTITY_GPU_FLAG_HAS_SKELETON)) {
            mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

            position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
        } else {
#endif
            position = object.model_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix));
#ifdef VERTEX_SKINNING_ENABLED
        }
#endif
    // }

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
    v_camera_position = camera.position.xyz;

    v_tangent = normalize(normal_matrix * vec4(a_tangent, 0.0)).xyz;
    v_bitangent = normalize(normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_tbn_matrix = mat3(v_tangent, v_bitangent, v_normal);

    mat4 projection_matrix = camera.projection;
    mat4 view_matrix = current_env_probe.face_view_matrices[gl_ViewIndex];

    v_view_space_position = view_matrix * position;

    v_object_index = OBJECT_INDEX;

    v_env_probe_extent = current_env_probe.aabb_max.xyz - current_env_probe.aabb_min.xyz;

    gl_Position = projection_matrix * view_matrix * position;

    v_cube_face_index = gl_ViewIndex;
    v_cube_face_uv = gl_Position.xy * 0.5 + 0.5;
}
