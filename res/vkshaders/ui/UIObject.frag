#version 450

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_screen_space_position;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"

void main()
{
    vec4 background = Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 6.0);

    vec4 ui_color = vec4(0.0, 0.005, 0.015, 0.8);

    vec4 result = vec4(
        (ui_color.rgb * ui_color.a) + (background.rgb * (1.0 - ui_color.a)),
        1.0
    );

    gbuffer_albedo = result;
}
