#version 450

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_screen_space_position;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"

void main()
{
    vec4 background = Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 5.0);

    vec4 ui_color = vec4(0.0, 0.005, 0.015, 0.9);

    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        // ivec2 texture_size = textureSize(sampler2D(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), texture_sampler), 0);
        vec4 albedo_texture = Texture2D(HYP_SAMPLER_NEAREST, GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), v_texcoord0);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD
            || /* font map: */ albedo_texture.r < MATERIAL_ALPHA_DISCARD) {
            discard;
        }

        ui_color = albedo_texture;
    }

    vec4 result = vec4(
        (ui_color.rgb * ui_color.a) + (background.rgb * (1.0 - ui_color.a)),
        1.0
    );

    gbuffer_albedo = result;
    // gbuffer_albedo = vec4(v_texcoord0, 0.0, 1.0);
}
