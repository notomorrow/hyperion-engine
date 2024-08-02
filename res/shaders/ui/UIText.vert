#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec2 v_texcoord0;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec3 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/env_probe.inc"

struct UITextCharacterInstance
{
    mat4    transform;
    vec2    texcoord_start;
    vec2    texcoord_end;
};

HYP_DESCRIPTOR_SSBO(UITextDescriptorSet, CharacterInstanceBuffer, standard = std430) readonly buffer CharacterInstanceBuffer
{
    UITextCharacterInstance characters[];
};

HYP_DESCRIPTOR_CBUFF(UITextDescriptorSet, UITextUniforms, standard = std430) uniform UITextUniforms
{
    mat4    projection_matrix;
    vec2    text_aabb_min;
    vec2    text_aabb_max;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    const int instance_id = gl_InstanceIndex;

    UITextCharacterInstance character_instance = characters[instance_id];

    vec2 text_aabb_extent = text_aabb_max - text_aabb_min;

    vec4 position = character_instance.transform * ((vec4((a_position * 0.5 + 0.5), 1.0))) / vec4(vec3(text_aabb_extent, 1.0), 1.0);
    v_position = position.xyz;

    vec2 texcoord_start = characters[instance_id].texcoord_start;
    vec2 texcoord_end = characters[instance_id].texcoord_end;
    vec2 texcoord_size = texcoord_end - texcoord_start;

    v_texcoord0 = texcoord_start + a_texcoord0 * texcoord_size;

    gl_Position = projection_matrix * position;
} 