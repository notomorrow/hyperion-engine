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
layout(location=12) in flat uint v_env_probe_index;
layout(location=13) in flat vec3 v_env_probe_extent;
layout(location=14) in flat uint v_cube_face_index;
layout(location=15) in vec2 v_cube_face_uv;

layout(location=0) out vec4 output_color;

#include "include/scene.inc"
#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/object.inc"
#include "include/packing.inc"

#ifdef SHADOWS
    #include "include/shadows.inc"
#endif

#include "include/tonemap.inc"
#include "include/PhysicalCamera.inc"

#define HYP_CUBEMAP_AMBIENT 0.1

vec3 GetCubemapCoord(uint face, vec2 uv)
{
    vec3 dir = vec3(0.0);

    switch (face) {
    case 0:
        dir = vec3(1.0,  -uv.y, -uv.x);
        break;  // POSITIVE X
    case 1:
        dir = vec3(-1.0,  -uv.y,  uv.x);
        break;  // NEGATIVE X
    case 2:
        dir = vec3(uv.x,  1.0,  uv.y);
        break;  // POSITIVE Y
    case 3:
        dir = vec3(uv.x, -1.0, -uv.y);
        break;  // NEGATIVE Y
    case 4:
        dir = vec3(uv.x,  -uv.y,  1.0);
        break;  // POSITIVE Z
    case 5:
        dir = vec3(uv.x,  -uv.y, -1.0);
        break;  // NEGATIVE Z
    }

    return dir;
}

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);

    vec4 view_position = scene.view * vec4(v_position, 1.0);
    view_position /= view_position.w;

    const float min_extent = min(abs(v_env_probe_extent.x), min(abs(v_env_probe_extent.y), abs(v_env_probe_extent.z))) * 0.5;

    vec4 albedo = CURRENT_MATERIAL.albedo;

    const float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    albedo *= (1.0 - metalness);

    vec2 texcoord = v_texcoord0 * CURRENT_MATERIAL.uv_scale;

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_ALBEDO_map, texcoord);

        if (albedo_texture.a < 0.2) {
            discard;
        }

        albedo *= albedo_texture;
    }

#if defined(LIGHTING) || defined(SHADOWS)
    vec3 L = light.position_intensity.xyz;
    L -= v_position.xyz * float(min(light.type, 1));
    L = normalize(L);

    float NdotL = max(0.0001, dot(N, L));
#endif

    vec4 previous_value = vec4(0.0);
    // float hysteresis = 0.0;


    // EnvProbe probe = GET_GRID_PROBE(v_env_probe_index);

    // if (probe.texture_index != ~0u) {
    //     const uint probe_texture_index = max(0, min(probe.texture_index, HYP_MAX_BOUND_ENV_PROBES - 1));

    //     hysteresis = 0.65;

    //     vec3 coord = GetCubemapCoord(v_cube_face_index, v_cube_face_uv);
    //     previous_value = TextureCube(HYP_SAMPLER_LINEAR, env_probe_textures[probe_texture_index], coord);
    //     previous_value.rgb  = pow(previous_value.rgb, vec3(1.0 / 2.2));
    // }

    float shadow = 1.0;

#ifdef SHADOWS
    if (light.shadow_map_index != ~0u) {
        shadow = GetShadowStandard(light.shadow_map_index, v_position.xyz, vec2(0.0), NdotL);
    }
#endif

#ifdef LIGHTING
    output_color.rgb = albedo.rgb * (1.0 / HYP_FMATH_PI) * exposure * vec3(HYP_CUBEMAP_AMBIENT);
    output_color.rgb += albedo.rgb * exposure * NdotL * light.position_intensity.w;
#else
    output_color.rgb = albedo.rgb;
#endif

    output_color.a = shadow;

    output_color.rgb = TonemapReinhardSimple(output_color.rgb);
}
