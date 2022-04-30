#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

#include "include/gbuffer.inc"

layout(set = 1, binding = 4) uniform sampler2D deferred_result;
layout(set = 1, binding = 12) uniform sampler2D shadow_map;
layout(set = 1, binding = 16, rgba8) uniform image2D image_storage_test;

/* TMP */
layout(set = 9, binding = 1, rgba8) uniform image2D rt_image;

layout(location=0) out vec4 out_color;

void main()
{
    vec2 texcoord = vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

    vec4 albedo = vec4(0.0);

    /* render last filter in the stack */
    out_color = imageLoad(rt_image, ivec2(int(v_texcoord0.x * 1024.0), int(v_texcoord0.y * 1024.0)));
    
    //if (out_color.a < 0.2) {
     //   out_color = vec4(texture(deferred_result, texcoord).rgb, 1.0);
    //}
}