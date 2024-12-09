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
layout(location=3) in vec2 v_texcoord1;
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
layout(location=6) out vec4 gbuffer_ws_normals;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;

#define texture_sampler sampler_linear

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#define PARALLAX_ENABLED 1
#define HAS_REFRACTION 1

#include "include/scene.inc"
#include "include/material.inc"
#include "include/object.inc"
#include "include/packing.inc"

#include "include/env_probe.inc"
#include "include/gbuffer.inc"

HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;

HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, size = 147456) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer, size = 4096) readonly buffer ShadowMapsBuffer
{
    ShadowMap shadow_map_data[16];
};

HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, count = 16) uniform texture2D shadow_maps[16];
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapTextures, count = 16) uniform textureCube point_shadow_maps[16];

#ifdef FORWARD_LIGHTING
#include "include/brdf.inc"
#include "deferred/DeferredLighting.glsl"
#include "include/shadows.inc"
#endif

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer, size = 33554432) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, LightsBuffer, size = 64) readonly buffer LightsBuffer
{
    Light light;
};

#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
HYP_DESCRIPTOR_SSBO(Object, MaterialsBuffer, size = 8388608) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

#ifndef CURRENT_MATERIAL
    #define CURRENT_MATERIAL (materials[object.material_index])
#endif
#else

HYP_DESCRIPTOR_SSBO_DYNAMIC(Object, MaterialsBuffer, size = 128) readonly buffer MaterialsBuffer
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

#if PARALLAX_ENABLED
#include "include/parallax.inc"
#endif

void main()
{
    vec3 view_vector = normalize(v_camera_position - v_position);
    vec3 N = normalize(v_normal);
    const vec3 ws_normals = N;
    const vec3 P = v_position.xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    
    vec3 tangent_view = transpose(v_tbn_matrix) * view_vector;
    vec3 tangent_position = v_tbn_matrix * v_position;
    
    gbuffer_albedo = CURRENT_MATERIAL.albedo;
    
    float ao = 1.0;
    float metalness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_METALNESS);
    float roughness = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ROUGHNESS);
    float transmission = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_TRANSMISSION);
    const float alpha_threshold = GET_MATERIAL_PARAM(CURRENT_MATERIAL, MATERIAL_PARAM_ALPHA_THRESHOLD);

    const float perceptual_roughness = sqrt(roughness);

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
        
        if (albedo_texture.a < alpha_threshold) {
            discard;
        }

        gbuffer_albedo *= albedo_texture;
    }

    gbuffer_albedo.a = max(gbuffer_albedo.a, 0.005);

    vec4 normals_texture = vec4(0.0);

    if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP)) {
        normals_texture = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP, texcoord) * 2.0 - 1.0;
        normals_texture.xy *= normal_map_intensity;
        normals_texture.xyz = normalize(normals_texture.xyz);

        N = v_tbn_matrix * normals_texture.xyz;
        N = normalize(N);

        // normals_texture.xy = (2.0 * (vec2(1.0) - SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_NORMAL_MAP, texcoord).rg) - 1.0);
        // normals_texture.z = sqrt(1.0 - dot(normals_texture.xy, normals_texture.xy));
        // N = ((normalize(v_tangent) * normals_texture.x) + (normalize(v_bitangent) * normals_texture.y) + (N * normals_texture.z));
    }

#ifdef FORWARD_LIGHTING
    {
        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        const vec3 F0 = CalculateF0(gbuffer_albedo.rgb, metalness);
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));

        vec3 L = light.position_intensity.xyz;
        L -= v_position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);
        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        const vec3 R = normalize(reflect(-V, N));

        // gbuffer_albedo.rgb *= NdotL;
        // gbuffer_albedo.rgb *= light.position_intensity.w;


        vec3 Ft = vec3(0.0);
        vec3 Fr = vec3(0.0);
        vec3 Fd = vec3(0.0);

        vec3 irradiance = vec3(0.0);
        vec4 ibl = vec4(0.0);
        vec4 reflections = vec4(0.0);

        { // irradiance
            const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);

            const vec3 dfg = CalculateDFG(F, roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

#if 1
            Ft = CalculateRefraction(
                uvec2(512, 512), // TODO: Make global constant
                P, N, V,
                texcoord,
                F0, E,
                transmission, roughness,
                vec4(0.0),
                gbuffer_albedo,
                vec3(ao)
            );
#endif

// #ifdef ENV_GRID_ENABLED
            // CalculateEnvProbeIrradiance(P, N, irradiance);
// #endif

            Fd = gbuffer_albedo.rgb * irradiance * (1.0 - E) * ao;

// #ifdef REFLECTION_PROBE_ENABLED
            ibl = CalculateReflectionProbe(current_env_probe, P, N, R, camera.position.xyz, roughness);
            ibl.a = saturate(ibl.a);
// #endif

// #ifdef SSR_ENABLED
//             CalculateScreenSpaceReflection(deferred_params, texcoord, depth, reflections);
// #endif

            // vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
            // specular_ao *= energy_compensation;

            vec3 reflection_combined = (ibl.rgb * (1.0 - reflections.a)) + (reflections.rgb);
            Fr = reflection_combined * E;
        }

        Ft *= transmission;
        Fd *= (1.0 - transmission);

        vec3 indirect_lighting = Ft + Fd + Fr;
        vec3 direct_lighting = vec3(0.0);

        { // direct lighting
            float shadow = 1.0;

            if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
                shadow = GetShadow(light.shadow_map_index, P, texcoord, camera.dimensions.xy, NdotL);
            }

            vec3 light_color = UINT_TO_VEC4(light.color_encoded).rgb;

            const float D = CalculateDistributionTerm(roughness, NdotH);
            const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
            const vec3 F = CalculateFresnelTerm(F0, roughness, LdotH);

            const vec3 dfg = CalculateDFG(F, roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            const vec3 diffuse_color = CalculateDiffuseColor(gbuffer_albedo.rgb, metalness);
            const vec3 specular_lobe = D * G * F;

            vec3 specular = specular_lobe;

            const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
                ? GetSquareFalloffAttenuation(P, light.position_intensity.xyz, light.radius)
                : 1.0;

            vec3 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
            vec3 diffuse = diffuse_lobe;
            diffuse *= (1.0 - transmission);

            vec3 direct_component = diffuse + specular * energy_compensation;

            direct_lighting += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        }

        vec3 lighting = indirect_lighting + direct_lighting;

        // overwrite gbuffer_albedo with lit result
        gbuffer_albedo.rgb = lighting;
    }
#endif

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


    // // TEMP testing lightmaps

    // vec4 irradiance = vec4(0.0);
    // vec4 radiance = vec4(0.0);

    // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_IRRADIANCE_MAP)) {
    //     irradiance = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_IRRADIANCE_MAP, vec2(v_texcoord1.x, 1.0 - v_texcoord1.y));
    // }

    // // if (HAS_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_RADIANCE_MAP)) {
    // //    radiance = SAMPLE_TEXTURE(CURRENT_MATERIAL, MATERIAL_TEXTURE_RADIANCE_MAP, vec2(v_texcoord1.x, 1.0 - v_texcoord1.y));
    // // }

    // gbuffer_albedo = (irradiance + radiance) * gbuffer_albedo;
    // gbuffer_albedo.a = 0.0; 

    gbuffer_normals = EncodeNormal(N);
    gbuffer_material = vec4(roughness, metalness, transmission, ao);
    gbuffer_tangents = vec4(PackNormalVec2(v_tangent), PackNormalVec2(v_bitangent));
    gbuffer_velocity = velocity;
    gbuffer_mask = UINT_TO_VEC4(v_object_mask);
    gbuffer_ws_normals = EncodeNormal(ws_normals);
}
