#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#extension GL_EXT_scalar_block_layout     : enable

#include "../include/defines.inc"

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in float g_lighting;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/material.inc"

#include "../include/vct/shared.inc"

layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = 0) uniform sampler2D textures[];
layout(set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 0, rgba16f) uniform image3D voxel_image;

void main()
{
    vec4 frag_color = material.albedo;
    
    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = texture(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), g_texcoord);
        
        frag_color *= albedo_texture;
    }

    frag_color.rgb *= vec3(max(0.25, g_lighting));
    frag_color.a = 1.0;

	imageStore(voxel_image, ivec3(VctStoragePosition(g_position)), frag_color);
}
