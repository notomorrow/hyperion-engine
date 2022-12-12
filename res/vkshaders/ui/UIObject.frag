#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_screen_space_position;
layout(location=2) in vec2 v_texcoord0;
layout(location=3) in flat uint v_object_index;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=5) out vec4 gbuffer_mask;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/material.inc"
#include "../include/object.inc"

void main()
{
    vec4 background = Texture2DLod(gbuffer_sampler, gbuffer_mip_chain, v_screen_space_position.xy, 5.0);

    vec4 ui_color = vec4(0.0, 0.035, 0.045, 0.75);
    //vec4 ui_color = vec4(0.8, 0.75, 0.7, 0.6);

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        // ivec2 texture_size = textureSize(sampler2D(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), texture_sampler), 0);
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_texcoord0);
        
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

    uint mask = GET_OBJECT_BUCKET(object);
    mask |= OBJECT_MASK_UI_BUTTON;

    gbuffer_albedo = result;
    gbuffer_mask = UINT_TO_VEC4(mask);
}
