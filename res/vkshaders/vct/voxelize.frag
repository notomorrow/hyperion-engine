#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#ifdef MODE_TEXTURE_3D
    #define HYP_VCT_MODE 1
#elif defined(MODE_SVO)
    #define HYP_VCT_MODE 2
#endif

#include "../include/defines.inc"

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in vec3 g_voxel;
layout(location=4) in float g_lighting;
layout(location=5) in flat uint g_object_index;

#define v_object_index g_object_index

#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/shadows.inc"
#include "../include/material.inc"
#include "../include/object.inc"

#include "../include/vct/shared.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 70, rgba8) uniform image3D voxel_image;

#define HYP_VCT_SAMPLE_ALBEDO_MAP 1
#define HYP_VCT_LIGHTING 1
#define HYP_VCT_LIGHTING_AMBIENT 0.01

#include "../include/vct/Voxelize.inc"

void main()
{
    vec4 frag_color = CURRENT_MATERIAL.albedo;

#ifdef MODE_SVO
    const uint voxel_id = CreateVoxelID();

    if (!bool(count_mode))
#endif
    {
#if HYP_VCT_SAMPLE_ALBEDO_MAP
        if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
            vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, g_texcoord * CURRENT_MATERIAL.uv_scale);
            
            frag_color *= albedo_texture;
        }
#endif

        frag_color.a = 1.0;

#if HYP_VCT_LIGHTING
        float shadow = 1.0;

        vec3 L = light.position_intensity.xyz;
        L -= g_position.xyz * float(min(light.type, 1));
        L = normalize(L);
        const float NdotL = dot(g_normal, L);

        if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
            shadow = GetShadowStandard(light.shadow_map_index, g_position.xyz, vec2(0.0), NdotL);
        }

        frag_color.a = max(HYP_VCT_LIGHTING_AMBIENT, NdotL * shadow);
#endif

        frag_color.rgb *= 1.0 - GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
        frag_color.rgb = pow(frag_color.rgb, vec3(1.0 / 2.2));

#ifdef MODE_SVO
        WriteVoxel(voxel_id, g_voxel, frag_color);
#else
        WriteVoxel(g_voxel, frag_color);
#endif
    }
}
