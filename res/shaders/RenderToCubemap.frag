#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=7) in flat vec3 v_camera_position;
layout(location=11) in flat uint v_object_index;
layout(location=12) in flat vec3 v_env_probe_extent;
layout(location=13) in flat uint v_cube_face_index;

layout(location=0) out vec4 output_color;

#ifdef WRITE_NORMALS
    layout(location=1) out vec4 output_normals;
    #ifdef WRITE_MOMENTS
        layout(location=2) out vec4 output_moments;
    #endif
#else
    #ifdef WRITE_MOMENTS
        layout(location=1) out vec4 output_moments;
    #endif
#endif

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/scene.inc"
#include "include/shared.inc"
#include "include/material.inc"
#include "include/gbuffer.inc"
#include "include/env_probe.inc"
#include "include/Octahedron.glsl"
#include "include/object.inc"
#include "include/packing.inc"
#include "include/brdf.inc"

#define HYP_CUBEMAP_AMBIENT 0.025

HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer) readonly buffer ShadowMapsBuffer
{
    ShadowMap shadow_map_data[16];
};

HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, count = 16) uniform texture2D shadow_maps[16];
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapTextures, count = 16) uniform textureCube point_shadow_maps[16];

#ifdef SHADOWS
    #include "include/shadows.inc"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentLight) readonly buffer CurrentLight
{
    Light light;
};

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material material;
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL material
#endif
#endif

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
HYP_DESCRIPTOR_SRV(Material, Textures, count = 16) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
#else
HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];
#endif

void main()
{
    vec3 V = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);
    vec3 R = reflect(-V, N);

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
    // Write distance, mean distance for variance.
    const float dist = distance(v_position, current_env_probe.world_position.xyz);

    vec2 moments = vec2(dist, HYP_FMATH_SQR(dist));
#endif

#ifdef MODE_SHADOWS
    float dx = dFdx(dist);
    float dy = dFdy(dist);
    
    moments.y += 0.25 * (HYP_FMATH_SQR(dx) + HYP_FMATH_SQR(dy));

    output_color = vec4(moments, 0.0, 1.0);
#else

    const float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    const float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);

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
    vec4 light_color = UINT_TO_VEC4(light.color_encoded);

    const vec3 H = normalize(L + V);

    const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
    const float NdotH = max(HYP_FMATH_EPSILON, dot(N, H));
    const float LdotH = max(HYP_FMATH_EPSILON, dot(L, H));
    const float HdotV = max(HYP_FMATH_EPSILON, dot(H, V));

    const vec3 F0 = CalculateF0(albedo.rgb, metalness);
    vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));

    vec3 indirect_lighting = vec3(0.0);
    vec3 direct_lighting = vec3(0.0);

    { // indirect
        const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);
        const vec3 dfg = CalculateDFG(F, roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

        vec3 Fd = albedo.rgb * vec3(HYP_CUBEMAP_AMBIENT) * (1.0 - E);

        indirect_lighting = Fd;
    }

    { // direct
        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const vec3 F = CalculateFresnelTerm(F0, roughness, LdotH);

        const vec3 dfg = CalculateDFG(F, roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

        const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
        const vec3 specular_lobe = D * G * F;

        vec3 specular = specular_lobe;

        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(v_position, light.position_intensity.xyz, light.radius)
            : 1.0;

        vec3 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        vec3 diffuse = diffuse_lobe;

        vec3 direct_component = diffuse + specular * energy_compensation;

        direct_lighting += direct_component * (light_color.rgb * NdotL * shadow * attenuation * light.position_intensity.w);
    }

    output_color.rgb = indirect_lighting + direct_lighting;
#else
    output_color.rgb = albedo.rgb;

    #ifdef SHADOWS
        output_color.rgb *= shadow;
    #endif
#endif

    output_color.a = 1.0;
#endif

#ifdef WRITE_NORMALS
    output_normals = vec4(PackNormalVec2(N), 0.0, 1.0);//vec4(1.0, 1.0, 0.0, 1.0);
#endif

#ifdef WRITE_MOMENTS
    output_moments = vec4(moments, 0.0, 1.0);
#endif
}
