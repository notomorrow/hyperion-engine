#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../include/defines.inc"

layout(location=0) in vec3 g_position;
layout(location=1) in vec3 g_normal;
layout(location=2) in vec2 g_texcoord;
layout(location=3) in vec3 g_voxel_pos;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/material.inc"
#include "../include/defines.inc"

#include "../include/voxel/shared.inc"

layout(std140, set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 0) buffer AtomicCounter {
    uint counter;
};

struct Fragment { uint x; uint y; };

layout(std430, set = HYP_DESCRIPTOR_SET_VOXELIZER, binding = 1) writeonly buffer FragmentListBuffer {
    Fragment fragments[];
};

layout(push_constant) uniform Constants {
    uint grid_size;
    uint count_mode;
};

#define HYP_VCT_MODE HYP_VCT_MODE_SVO
#include "../include/vct/Voxelize.inc"

#define VOXEL_GRID_SCALE 1.0

void main()
{
    const uint voxel_id = CreateVoxelID();

    if (!bool(count_mode)) {
        vec4 frag_color = material.albedo;
        
        if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
            vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, g_texcoord);
            
            frag_color *= albedo_texture;
        }
        
        /* basic n•l */
        // vec3 L = normalize(scene.light_direction.xyz);
        // vec3 N = normalize(g_normal);
        
        // float NdotL = max(0.0001, dot(N, L));
        
        //frag_color.rgb *= NdotL;

        WriteVoxel(voxel_id, g_voxel_pos, frag_color);
    }
}
