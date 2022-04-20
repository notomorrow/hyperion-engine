#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

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


#include "include/scene.inc"
#include "include/material.inc"
#include "include/parallax.inc"


layout(set = 6, binding = 0) uniform sampler2D textures[];
layout(set = 6, binding = 0) uniform samplerCube cubemap_textures[];

void main()
{
    vec3 L = normalize(v_light_direction);
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    float NdotL = dot(normal, L);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * -v_view_space_position;
    vec3 tangent_light = v_tbn_matrix * L;
    vec3 tangent_position = v_tbn_matrix * v_position;

    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = material.albedo;
    
    vec2 texcoord = v_texcoord0;
    //texcoord.y = 1.0 - texcoord.y;
    
    if (HasMaterialTexture(3)) {
        vec2 parallax_texcoord = ParallaxMappedTexCoords(
            textures[material.texture_index[3]],
            0.025, //material.parallax_height,
            texcoord,
            normalize(tangent_view)
        );
        
        texcoord = parallax_texcoord;
    }
    
    if (HasMaterialTexture(0)) {
        vec4 albedo_texture = texture(textures[material.texture_index[0]], texcoord);
        
        if (albedo_texture.a < 0.2) {
            discard;
        }
        
        gbuffer_albedo *= albedo_texture;
    }
        
    
    if (HasMaterialTexture(1)) {
        vec4 normals_texture = texture(textures[material.texture_index[1]], texcoord) * 2.0 - 1.0;
        normal = normalize(v_tbn_matrix * normals_texture.rgb);
    }
    
    gbuffer_normals = vec4(normal, 1.0);
    gbuffer_positions = vec4(v_position, 1.0);
}