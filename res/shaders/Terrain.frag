#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_samplerless_texture_functions : enable

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

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=4) out vec2 gbuffer_velocity;
layout(location=5) out vec4 gbuffer_mask;

#define PARALLAX_ENABLED 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/object.inc"
#include "include/packing.inc"

#if PARALLAX_ENABLED
#include "include/parallax.inc"
#endif

//tmp
#include "include/aabb.inc"

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_position = v_tbn_matrix * v_position;

    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = CURRENT_MATERIAL.albedo;
    gbuffer_albedo.a = 1.0;
    
    float ao  = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    
    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;
    
#if 0//PARALLAX_ENABLED
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
        vec4 albedo_texture = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, v_position, normal);
        
        // if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
        //     discard;
        // }

        gbuffer_albedo = vec4(albedo_texture.rgb, 1.0);
    }


    // temp grass color
    // gbuffer_albedo.rgb = vec3(0.5, 0.8, 0.35) * 0.15;

    // lerp to rock based on slope
    // gbuffer_albedo.rgb = mix(gbuffer_albedo.rgb, vec3(0.11), 1.0 - smoothstep(0.45, 0.65, abs(dot(normal, vec3(0.0, 1.0, 0.0)))));

    // gbuffer_albedo.a = 0.0;// no lighting for now

    vec4 normals_texture = vec4(0.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP)) {
        normals_texture = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP, v_position, normal) * 2.0 - 1.0;
        normal = normalize(v_tbn_matrix * normals_texture.rgb);
    }

    // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP)) {
    //     float metalness_sample = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_METALNESS_MAP, texcoord).r;
        
    //     metalness = metalness_sample;//mix(metalness, metalness_sample, metalness_sample);
    // }
    
    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ROUGHNESS_MAP)) {
        float roughness_sample = SAMPLE_TEXTURE_TRIPLANAR(CURRENT_MATERIAL, MATERIAL_TEXTURE_ROUGHNESS_MAP, v_position, normal).r;
        
        roughness = roughness_sample;//mix(roughness, roughness_sample, roughness_sample);
    }
    
    // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP)) {
    //     ao = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_AO_MAP, texcoord).r;
    // }

    // gbuffer_albedo.rgb = GetTriplanarBlend(normal);

    vec2 velocity = vec2(((v_position_ndc.xy / v_position_ndc.w) * 0.5 + 0.5) - ((v_previous_position_ndc.xy / v_previous_position_ndc.w) * 0.5 + 0.5));

    gbuffer_normals = EncodeNormal(normal);
    gbuffer_material = vec4(roughness, metalness, transmission, ao);
    gbuffer_velocity = velocity;
    gbuffer_mask = UINT_TO_VEC4(GET_OBJECT_BUCKET(object) | OBJECT_MASK_TERRAIN);
}
