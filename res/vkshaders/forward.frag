#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=6) in flat vec3 v_light_direction;
layout(location=7) in flat vec3 v_camera_position;
layout(location=8) in mat3 v_tbn_matrix;
layout(location=12) in vec3 v_view_space_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;
layout(location=3) out vec4 gbuffer_material;


#define PARALLAX_ENABLED 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/object.inc"
#include "include/packing.inc"

#if PARALLAX_ENABLED
#include "include/parallax.inc"
#endif

void main()
{
    vec3 L = normalize(v_light_direction);
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, L);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_light = v_tbn_matrix * L;
    vec3 tangent_position = v_tbn_matrix * v_position;

    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = material.albedo;
    
    float ao        = 1.0;
    float metalness = material.metalness;
    float roughness = material.roughness;
    
    vec2 texcoord = v_texcoord0;
    
#if PARALLAX_ENABLED
    if (HAS_TEXTURE(MATERIAL_TEXTURE_PARALLAX_MAP)) {
        vec2 parallax_texcoord = ParallaxMappedTexCoords(
            material.parallax_height,
            texcoord,
            normalize(tangent_view)
        );
        
        texcoord = parallax_texcoord;
    }
#endif

    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map, texcoord);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
        //    discard;
        }

        gbuffer_albedo *= albedo_texture;
    }

    if (HAS_TEXTURE(MATERIAL_TEXTURE_NORMAL_MAP)) {
        vec4 normals_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_NORMAL_MAP, texcoord) * 2.0 - 1.0;
        normal = normalize(v_tbn_matrix * normals_texture.rgb);
    }

    if (HAS_TEXTURE(MATERIAL_TEXTURE_METALNESS_MAP)) {
        float metalness_sample = SAMPLE_TEXTURE(MATERIAL_TEXTURE_METALNESS_MAP, texcoord).r;
        
        metalness = metalness_sample;//mix(metalness, metalness_sample, metalness_sample);
    }
    
    if (HAS_TEXTURE(MATERIAL_TEXTURE_ROUGHNESS_MAP)) {
        float roughness_sample = SAMPLE_TEXTURE(MATERIAL_TEXTURE_ROUGHNESS_MAP, texcoord).r;
        
        roughness = roughness_sample;//mix(roughness, roughness_sample, roughness_sample);
    }
    
    if (HAS_TEXTURE(MATERIAL_TEXTURE_AO_MAP)) {
        ao = SAMPLE_TEXTURE(MATERIAL_TEXTURE_AO_MAP, texcoord).r;
    }
    
    gbuffer_normals   = EncodeNormal(normal);
    gbuffer_positions = vec4(v_position, 1.0);
    gbuffer_material  = vec4(roughness, metalness, 0.0, ao);
}
