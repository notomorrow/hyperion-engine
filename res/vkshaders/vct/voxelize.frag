#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#extension GL_EXT_scalar_block_layout     : enable

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in float g_lighting;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/material.inc"

#include "../include/vct/shared.inc"

layout(set = 6, binding = 0) uniform sampler2D textures[];
layout(set = 8, binding = 0, rgba16f) uniform image3D voxel_image;

void main()
{
    vec4 frag_color = material.albedo;
    
    if (HasMaterialTexture(0)) {
        vec4 albedo_texture = texture(textures[material.texture_index[0]], g_texcoord);
        
        frag_color *= albedo_texture;
    }

    frag_color.rgb *= vec3(max(0.25, g_lighting));
    frag_color.a = 1.0;

	imageStore(voxel_image, ivec3(VctStoragePosition(g_position)), frag_color);
}