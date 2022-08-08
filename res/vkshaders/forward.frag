#version 450
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
layout(location=12) in vec3 v_view_space_position;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;
layout(location=3) out vec4 gbuffer_material;
layout(location=4) out vec4 gbuffer_tangents;
layout(location=5) out vec4 gbuffer_bitangents;


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

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 36) uniform texture2D depth_pyramid_result;


void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 normal = normalize(v_normal);
    float NdotV = dot(normal, view_vector);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_position = v_tbn_matrix * v_position;

    vec3 reflection_vector = reflect(view_vector, normal);
    
    gbuffer_albedo = material.albedo;
    
    float ao        = 1.0;
    float metalness = material.metalness;
    float roughness = material.roughness;
    
    vec2 texcoord = v_texcoord0 * material.uv_scale;
    
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
            discard;
        }

        gbuffer_albedo *= albedo_texture;
    }

    vec4 normals_texture = vec4(0.0);

    if (HAS_TEXTURE(MATERIAL_TEXTURE_NORMAL_MAP)) {
        normals_texture = SAMPLE_TEXTURE(MATERIAL_TEXTURE_NORMAL_MAP, texcoord) * 2.0 - 1.0;
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



    /*if (scene.environment_texture_usage != 0) {
        uint probe_index = scene.environment_texture_index;
        vec3 V = normalize(scene.camera_position.xyz - v_position.xyz);
        vec3 R = normalize(reflect(-V, normal.xyz));
        gbuffer_albedo.rgb = SampleProbeParallaxCorrected(gbuffer_sampler, env_probe_textures[probe_index], env_probes[probe_index], v_position.xyz, R, 7.0).rgb;   //TextureCubeLod(gbuffer_sampler, rendered_cubemaps[scene.environment_texture_index], R, lod).rgb;
    }*/

    AABB aabb;
    aabb.min = object.world_aabb_min.xyz;
    aabb.max = object.world_aabb_max.xyz;


    vec4 screenspace_aabb = vec4(-99.0);
    vec4 clip_pos = vec4(0.0);
    vec4 projected_corner = vec4(0.0);
    vec3 clip_min = vec3(1, 1, 1);
    vec3 clip_max = vec3(-1, -1, 0);
    float mip = 0.0;


    uint cull_bits = 999;

    // get view/proj matrices from scene.
    mat4 view = scene.view;
    mat4 proj = scene.projection;

    projected_corner = proj * view * vec4(AABBGetCorner(aabb, 0), 1.0);

    clip_pos = projected_corner;
    clip_pos.z = min(1.0, max(clip_pos.z, 0.0));
    clip_pos.xyz /= clip_pos.w;
    clip_pos.xy = clamp(clip_pos.xy, vec2(-1.0), vec2(1.0));

    clip_min = clip_pos.xyz;
    clip_max = clip_min;

    // transform worldspace aabb to screenspace
    for (int i = 1; i < 8; i++) {
        projected_corner = proj * view * vec4(AABBGetCorner(aabb, i), 1.0);

        clip_pos = projected_corner;
        clip_pos.z = min(1.0, max(clip_pos.z, 0.0));
        clip_pos.xyz /= clip_pos.w;
        clip_pos.xy = clamp(clip_pos.xy, vec2(-1.0), vec2(1.0));

        clip_min = min(clip_pos.xyz, clip_min);
        clip_max = max(clip_pos.xyz, clip_max);
    }

    //clip_min.xy = clip_min.xy * vec2(0.5) + vec2(0.5);
    //clip_max.xy = clip_max.xy * vec2(0.5) + vec2(0.5);


    vec2 viewport = vec2(textureSize(depth_pyramid_result, 0));
    vec2 scr_pos_min = (clip_min.xy * .5f + .5f) * viewport;
    vec2 scr_pos_max = (clip_max.xy * .5f + .5f) * viewport;
    vec2 scr_rect = (clip_max.xy - clip_min.xy) * .5f * viewport;
    float scr_size = max(scr_rect.x, scr_rect.y);
    int mip_level = int(ceil(log2(scr_size)));
    uvec2 dim = (uvec2(scr_pos_max) >> mip_level) - (uvec2(scr_pos_min) >> mip_level);
    int use_lower = int(step(dim.x, 2.f) * step(dim.y, 2.f));
    mip_level = use_lower * max(0, mip_level - 1) + (1 - use_lower) * mip_level;

    vec2 uv_scale = vec2(uvec2(textureSize(depth_pyramid_result, 0)) >> mip_level) / textureSize(depth_pyramid_result, 0) / vec2(1024 >> mip_level);
    vec2 uv_min = scr_pos_min * uv_scale;
    vec2 uv_max = scr_pos_max * uv_scale;

    // Calculate hi-Z buffer mip
    // vec2 size = (clip_max.xy - clip_min.xy) * textureSize(depth_pyramid_result, 0);
    // mip = ceil(log2(max(size.x, size.y)));
    
    
    mip = float(mip_level);

    /*vec4 fragment_position = scene.projection * scene.view * vec4(v_position.xyz, 1.0);
    fragment_position.xy = fragment_position.xy * vec2(0.5) + vec2(0.5);

    if (clip_min.z <= fragment_position.z)
    {
        discard;
    }*/

    const vec4 depths = {
        Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, uv_min.xy, mip).r,
        Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, vec2(uv_min.x, uv_max.y), mip).r,
        Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, vec2(uv_max.x, uv_min.y), mip).r,
        Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, uv_max.xy, mip).r
    };

    
    // const vec4 depths = {
    //     Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, clip_min.xy * 0.5 + 0.5, mip).r,
    //     Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, vec2(clip_max.x, clip_min.y) * 0.5 + 0.5, mip).r,
    //     Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, clip_max.xy * 0.5 + 0.5, mip).r,
    //     Texture2DLod(gbuffer_depth_sampler, depth_pyramid_result, vec2(clip_min.x, clip_max.y) * 0.5 + 0.5, mip).r
    // };

    float max_depth = max(0, max(max(max(depths.x, depths.y), depths.z), depths.w));
    //screenspace_aabb = vec4(clip_min.xy, clip_max.xy);
    bool is_visible = clip_min.z <= max_depth;
    //gbuffer_albedo = vec4(max_depth-clip_min.z, 0, float(!is_visible), 0);//vec4(clip_max.xy - clip_min.xy, 0, 0);
    // gbuffer_albedo = ReconstructWorldSpacePositionFromDepth(inverse(proj * view), clip_min.xy, depths[0]);
    // gbuffer_albedo.a = 0.0;
    // gbuffer_albedo = vec4(uv_min, 0, 0);
    
    gbuffer_normals    = EncodeNormal(normal);
    gbuffer_positions  = vec4(v_position, 1.0);
    gbuffer_material   = vec4(roughness, metalness, 0.0, ao);
    gbuffer_tangents   = EncodeNormal(v_tangent);
    gbuffer_bitangents = EncodeNormal(v_bitangent);
}
