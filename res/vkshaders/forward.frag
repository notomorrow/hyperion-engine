#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=7) in flat vec3 v_camera_position;
layout(location=8) in mat3 v_tbn_matrix;
layout(location=11) in vec4 v_position_ndc;
layout(location=12) in vec4 v_previous_position_ndc;
layout(location=15) in flat uint v_object_index;
layout(location=16) in flat uint v_object_mask;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=3) out vec4 gbuffer_tangents;
layout(location=4) out vec2 gbuffer_velocity;
layout(location=5) out vec4 gbuffer_mask;

#define PARALLAX_ENABLED 1
#define HAS_REFRACTION 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/object.inc"
#include "include/packing.inc"

#if PARALLAX_ENABLED
#include "include/parallax.inc"
#endif

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_position = v_tbn_matrix * v_position;
    
    gbuffer_albedo = CURRENT_MATERIAL.albedo;
    
    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);

    const float normal_map_intensity = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_NORMAL_MAP_INTENSITY);
    
    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;
    
#if PARALLAX_ENABLED
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_PARALLAX_MAP)) {
        vec2 parallax_texcoord = ParallaxMappedTexCoords(
            CURRENT_MATERIAL.parallax_height,
            texcoord,
            normalize(tangent_view)
        );
        
        texcoord = parallax_texcoord;
    }
#endif

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, texcoord);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
            discard;
        }

        gbuffer_albedo *= albedo_texture;
    }

    vec4 normals_texture = vec4(0.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP)) {
        normals_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP, texcoord) * 2.0 - 1.0;
        normals_texture.xy *= normal_map_intensity;
        normals_texture.xyz = normalize(normals_texture.xyz);

        normal = v_tbn_matrix * normals_texture.xyz;
        normal = normalize(normal);


        // normals_texture.xy = (2.0 * (vec2(1.0) - SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP, texcoord).rg) - 1.0);
        // normals_texture.z = sqrt(1.0 - dot(normals_texture.xy, normals_texture.xy));
        // normal = ((normalize(v_tangent) * normals_texture.x) + (normalize(v_bitangent) * normals_texture.y) + (normal * normals_texture.z));
    }

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP)) {
        float metalness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP, texcoord).r;
        
        metalness = metalness_sample;
    }
    
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ROUGHNESS_MAP)) {
        float roughness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ROUGHNESS_MAP, texcoord).r;
        
        roughness = roughness_sample;
    }
    
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP)) {
        ao = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP, texcoord).r;
    }

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));

    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(0.0, metalness, transmission, ao);
    gbuffer_tangents = vec4(PackNormalVec2(v_tangent), PackNormalVec2(v_bitangent));
    gbuffer_velocity = velocity;
    gbuffer_mask = UINT_TO_VEC4(v_object_mask);
}
