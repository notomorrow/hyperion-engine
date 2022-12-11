#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../include/defines.inc"

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in vec3 g_voxel;
layout(location=4) in float g_lighting;
layout(location=5) in flat uint g_object_index;

#define v_object_index g_object_index

#include "../include/scene.inc"
#include "../include/material.inc"

#define HYP_INSTANCING
#include "../include/object.inc"

#include "../include/vct/shared.inc"

layout(set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 0, rgba8) uniform image3D voxel_image;

#define HYP_VCT_SAMPLE_ALBEDO_MAP 1
#define HYP_VCT_LIGHTING 0
#define HYP_VCT_LIGHTING_AMBIENT 0.1

#define HYP_VCT_MODE HYP_VCT_MODE_TEXTURE
#include "../include/vct/Voxelize.inc"

void main()
{
    vec4 frag_color = CURRENT_MATERIAL.albedo;

#if HYP_VCT_SAMPLE_ALBEDO_MAP
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, g_texcoord);
        
        frag_color *= albedo_texture;
    }
#endif

#if HYP_VCT_LIGHTING
    vec3 L = light.position_intensity.xyz;
    L -= g_position.xyz * float(min(light.type, 1));
    L = normalize(L);
    frag_color.rgb *= vec3(max(HYP_VCT_LIGHTING_AMBIENT, Saturate(dot(g_normal, L))));//vec3(max(HYP_VCT_LIGHTING_AMBIENT, g_lighting));
#endif

    // frag_color.rgb = pow(frag_color.rgb, vec3(1.0 / 2.2));
    frag_color.a = 1.0;

	WriteVoxel(g_voxel, frag_color);
}
