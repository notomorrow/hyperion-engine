#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) out vec3 v_position;
layout (location = 1) out vec3 v_screen_space_position;
layout (location = 2) out vec2 v_texcoord0;
layout (location = 3) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;

#include "../include/scene.inc"

#define HYP_INSTANCING
#include "../include/object.inc"

#include "../include/UIObject.glsl"

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

void main()
{
    UIObjectProperties properties;
    GetUIObjectProperties(object, properties);

#ifdef TYPE_TEXT // Text uses a different sizing method. See UIText::UpdateSize
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
#else
    // scale the quad mesh to the size of the object
    vec4 position = object.model_matrix * vec4(a_position * vec3(vec2(properties.size.xy), 1.0), 1.0);
#endif

    vec4 ndc_position = camera.projection * camera.view * position;

    v_position = position.xyz;
    v_screen_space_position = vec3(ndc_position.xy * 0.5 + 0.5, ndc_position.z);
    v_texcoord0 = a_texcoord0;

    v_object_index = OBJECT_INDEX;

    gl_Position = ndc_position;

} 