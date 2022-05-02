#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout     : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

#include "include/gbuffer.inc"

#include "include/rt/probe/probe_uniforms.inc"

layout(set = 1, binding = 4) uniform sampler2D deferred_result;
layout(set = 1, binding = 12) uniform sampler2D shadow_map;
layout(set = 1, binding = 16, rgba8) uniform image2D image_storage_test;

layout(location=0) out vec4 out_color;

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo = vec4(0.0);

    ProbeRayData ray = GetProbeRayData(uvec2(uint(v_texcoord0.x * float(probe_system.image_dimensions.x)), uint(v_texcoord0.y * float(probe_system.image_dimensions.y))));
    
    //out_color = unpackUnorm4x8(ray.color_packed);

    /* render last filter in the stack */
    //out_color = imageLoad(rt_image, ivec2(int(v_texcoord0.x * float(probe_system.image_dimensions.x)), int(v_texcoord0.y * float(probe_system.image_dimensions.y))));
    
    //if (out_color.a < 0.2) {
        out_color = vec4(texture(deferred_result, texcoord).rgb, 1.0);
    //}
}