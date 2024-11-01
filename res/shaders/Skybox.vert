#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=3) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "include/scene.inc"

#define HYP_INSTANCING
#include "include/object.inc"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CameraShaderData
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO(Object, EntityInstanceBatchBuffer, size = 256) readonly buffer EntityInstanceBatchBuffer
{
    EntityInstanceBatch entity_instance_batch;
};

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    mat4 normal_matrix = transpose(inverse(object.model_matrix));

    v_position = a_position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

    mat4 view_matrix = camera.view;
    view_matrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    v_object_index = OBJECT_INDEX;

    gl_Position = camera.projection * view_matrix * position;
} 