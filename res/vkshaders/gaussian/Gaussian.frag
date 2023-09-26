#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_samplerless_texture_functions : enable

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/material.inc"
#include "../include/packing.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(location=0) in vec3 v_position;
layout(location=1) in vec4 v_color;
layout(location=2) in vec4 v_conic_radius;
layout(location=3) in vec2 v_center_screen_position;
layout(location=4) in vec2 v_quad_position;
layout(location=5) in vec2 v_uv;

layout(location = 0) out vec4 gbuffer_albedo;

void main()
{
    vec4 color = v_color;

    vec2 d = -v_uv;
    vec3 conic = v_conic_radius.xyz;
    float power = -0.5 * (conic.x * d.x * d.x + conic.z * d.y * d.y) + conic.y * d.x * d.y;

    if (power > 0.0) {
        discard;
    }

    float alpha = min(0.99, color.a * exp(power));

    gbuffer_albedo = vec4(color.rgb * alpha, alpha);
}
