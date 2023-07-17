#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=4) in vec3 v_tangent;
layout(location=5) in vec3 v_bitangent;
layout(location=7) in flat vec3 v_camera_position;
layout(location=8) in mat3 v_tbn_matrix;
layout(location=11) in flat uint v_object_index;
layout(location=12) in flat vec3 v_env_probe_extent;
layout(location=13) in flat uint v_cube_face_index;
layout(location=14) in vec2 v_cube_face_uv;

// #ifndef MODE_SHADOWS
layout(location=0) out vec4 output_color;
#ifdef WRITE_NORMALS
layout(location=1) out vec2 output_normals;
#ifdef WRITE_MOMENTS
layout(location=2) out vec2 output_moments;
#endif

#else
#ifdef WRITE_MOMENTS
layout(location=1) out vec2 output_moments;
#endif
#endif
// #endif

#include "include/scene.inc"
#include "include/shared.inc"
#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/object.inc"
#include "include/packing.inc"

#define HYP_CUBEMAP_AMBIENT 0.08

#ifdef MODE_AMBIENT
//    #define LIGHTING
//     #define SHADOWS
#endif

#ifdef MODE_REFLECTION
    #define LIGHTING
    #define SHADOWS
    // #define TONEMAP
#endif

#ifdef TONEMAP
    #include "include/tonemap.inc"
#endif

#ifdef SHADOWS
    #include "include/shadows.inc"
#endif

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);

    // vec4 view_position = camera.view * vec4(v_position, 1.0);
    // view_position.xyz /= view_position.w;

    const float min_extent = min(abs(v_env_probe_extent.x), min(abs(v_env_probe_extent.y), abs(v_env_probe_extent.z))) * 0.5;

    vec4 albedo = CURRENT_MATERIAL.albedo;

    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, texcoord);

        if (albedo_texture.a < 0.2) {
            discard;
        }

        albedo *= albedo_texture;
    }

#if defined(WRITE_MOMENTS) || defined(MODE_SHADOWS)
        
    // write out distance
    const vec3 env_probe_center = (current_env_probe.aabb_max.xyz + current_env_probe.aabb_min.xyz) * 0.5;
    const float shadow_depth = length(v_position.xyz);//distance(v_position.xyz, env_probe_center);

    vec2 moments = vec2(shadow_depth, HYP_FMATH_SQR(shadow_depth));

#endif

#ifdef MODE_SHADOWS

    float dx = dFdx(shadow_depth);
    float dy = dFdy(shadow_depth);
    
    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));
    output_color = vec4(moments, 0.0, 0.0);
#else

    const float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);

#if defined(LIGHTING) || defined(SHADOWS)
    vec3 L = light.position_intensity.xyz;
    L -= v_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    float NdotL = max(0.0001, dot(N, L));
#endif

    vec4 previous_value = vec4(0.0);

    float shadow = 1.0;

#ifdef SHADOWS
    if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
        shadow = GetShadowStandard(light.shadow_map_index, v_position.xyz, vec2(0.0), NdotL);
    }
#endif

#ifdef LIGHTING
    const vec4 light_color = unpackUnorm4x8(light.color_encoded);

    output_color.rgb = albedo.rgb * (1.0 - metalness) * vec3(HYP_CUBEMAP_AMBIENT);   

    // if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL) {
        #ifdef SHADOWS
            output_color.rgb += albedo.rgb * HYP_FMATH_ONE_OVER_PI * NdotL * light_color.rgb * light.position_intensity.w * shadow;
        #else
            output_color.rgb += albedo.rgb * HYP_FMATH_ONE_OVER_PI * NdotL * light_color.rgb * light.position_intensity.w;
        #endif
    // }
#else
    output_color.rgb = albedo.rgb;

    #ifdef SHADOWS
        output_color.rgb *= shadow;
    #endif
#endif

#ifdef TONEMAP
    output_color.rgb = TonemapReinhardSimple(output_color.rgb);
#endif

    output_color.a = 1.0;

#endif

#ifdef WRITE_NORMALS
    output_normals = PackNormalVec2(v_normal);
#endif

#ifdef WRITE_MOMENTS
    output_moments = moments;
#endif
}
