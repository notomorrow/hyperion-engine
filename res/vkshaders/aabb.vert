#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=11) out vec4 v_position_ndc;
layout(location=12) out vec4 v_previous_position_ndc;
layout(location=15) out flat uint v_object_index;

#ifdef IMMEDIATE_MODE
layout(location=16) out vec4 v_color;
#endif

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec3 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#include "include/scene.inc"

#ifdef IMMEDIATE_MODE
    layout(std140, set = 0, binding = 0, row_major) readonly buffer ImmediateModeDraw
    {
        mat4 model_matrix;
        uint color_packed;
    };

    #define MODEL_MATRIX (model_matrix)
    #define PREV_MODEL_MATRIX (model_matrix)
#else
    #include "include/object.inc"

    #define MODEL_MATRIX (object.model_matrix)
    #define PREV_MODEL_MATRIX (object.previous_model_matrix)
#endif

void main()
{
    vec4 position = MODEL_MATRIX * vec4(a_position, 1.0);
    vec4 previous_position = PREV_MODEL_MATRIX * vec4(a_position, 1.0);

    mat4 normal_matrix = transpose(inverse(MODEL_MATRIX));

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_tangent = (normal_matrix * vec4(a_tangent, 0.0)).xyz;
	v_bitangent = (normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

#ifndef IMMEDIATE_MODE
    v_object_index = OBJECT_INDEX;
#else
    v_object_index = ~0u; // unused
    v_color = UINT_TO_VEC4(color_packed);
#endif

    mat4 jitter_matrix = mat4(1.0);
    jitter_matrix[3][0] += camera.jitter.x;
    jitter_matrix[3][1] += camera.jitter.y;

    v_position_ndc = (jitter_matrix * camera.projection) * camera.view * position;
    v_previous_position_ndc = (jitter_matrix * camera.projection) * camera.previous_view * previous_position;

    gl_Position = camera.projection * camera.view * position;
} 