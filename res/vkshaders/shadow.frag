#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout     : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=2) in vec2 v_texcoord0;

layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = 0) uniform sampler2D textures[];

#include "include/material.inc"

void main()
{
    if (HasMaterialTexture(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = texture(textures[material.texture_index[MATERIAL_TEXTURE_ALBEDO_map]], v_texcoord0);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
            discard;
        }
    }
}
