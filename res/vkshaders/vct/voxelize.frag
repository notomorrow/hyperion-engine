#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier    : enable
#extension GL_EXT_scalar_block_layout     : enable

#include "../include/defines.inc"

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in vec3 g_voxel;
layout(location=4) in float g_lighting;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/material.inc"

#include "../include/vct/shared.inc"

layout(set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 0, rgba8) uniform image3D voxel_image;

#define HYP_VCT_SAMPLE_ALBEDO_MAP 1
#define HYP_VCT_LIGHTING 0
#define HYP_VCT_LIGHTING_AMBIENT 0.1

void main()
{
    vec4 frag_color = material.albedo;

#if HYP_VCT_SAMPLE_ALBEDO_MAP
    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, g_texcoord);
        
        frag_color *= albedo_texture;
    }
#endif

#if HYP_VCT_LIGHTING
    vec3 L = light.position.xyz;
    L -= g_position.xyz * float(min(light.type, 1));
    L = normalize(L);
    frag_color.rgb *= vec3(max(HYP_VCT_LIGHTING_AMBIENT, dot(g_normal, L)));//vec3(max(HYP_VCT_LIGHTING_AMBIENT, g_lighting));
#endif

    frag_color.a = 1.0;

	imageStore(voxel_image, ivec3(VctStoragePosition(g_voxel)), frag_color);
}
