#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 41) uniform texture2D ssao_gi_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 45) uniform texture2D rt_radiance_final;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 57) uniform texture2D env_grid_irradiance_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 58) uniform texture2D env_grid_radiance_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 59) uniform texture2D reflection_probes_texture;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/PostFXSample.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/PhysicalCamera.inc"

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#define HYP_VCT_REFLECTIONS_ENABLED 1
#define HYP_VCT_INDIRECT_ENABLED 1

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#include "include/DDGI.inc"

void main()
{
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    uint mask = VEC4_TO_UINT(SampleGBuffer(gbuffer_mask_texture, texcoord));
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
    vec3 result = vec3(0.0);

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(camera.position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    
    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);
    vec3 F = vec3(0.0);

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi_result, v_texcoord0);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);

    if (perform_lighting && !bool(mask & 0x10)) {
        const float roughness = material.r;
        const float metalness = material.g;

        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = CalculateDiffuseColor(albedo_linear, metalness);
        const vec3 F0 = CalculateF0(albedo_linear, metalness);

        F = CalculateFresnelTerm(F0, roughness, NdotV);

        const float perceptual_roughness = sqrt(roughness);
        const vec3 dfg = CalculateDFG(F, roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

        // @TODO: Add check
        // const float environment_map_lod = float(9.0) * perceptual_roughness * (2.0 - perceptual_roughness);
        // ibl.rgb = TextureCubeLod(HYP_SAMPLER_LINEAR, environment_maps[0], R, environment_map_lod).rgb;

#ifdef REFLECTION_PROBE_ENABLED
        vec4 reflection_probes_color = Texture2D(HYP_SAMPLER_LINEAR, reflection_probes_texture, texcoord);
        ibl.rgb = ibl * (1.0 - reflection_probes_color.a) + (reflection_probes_color.rgb);
#endif

        vec4 env_grid_radiance = Texture2D(HYP_SAMPLER_LINEAR, env_grid_radiance_texture, texcoord);
        ibl = ibl * (1.0 - env_grid_radiance.a) + (env_grid_radiance.rgb);
        
        irradiance += Texture2D(HYP_SAMPLER_LINEAR, env_grid_irradiance_texture, texcoord).rgb * ENV_PROBE_MULTIPLIER;

#ifdef SSR_ENABLED
        CalculateScreenSpaceReflection(deferred_params, texcoord, depth, ibl);
#endif

#ifdef RT_REFLECTIONS_ENABLED
        CalculateRaytracingReflection(deferred_params, texcoord, ibl);
#endif

#ifdef RT_GI_ENABLED
        irradiance += DDGISampleIrradiance(position.xyz, N, V).rgb;
#endif

#ifdef HBIL_ENABLED
        CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);
#endif

        vec3 Fd = diffuse_color.rgb * irradiance * (1.0 - E) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        specular_ao *= energy_compensation;

        vec3 Fr = ibl * E * specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
        Fd *= multibounce;
        Fr *= multibounce;

        // Fr *= exposure * IBL_INTENSITY;
        // Fd *= exposure * IBL_INTENSITY;

        result = Fd + Fr;

        vec4 final_result = vec4(result, 1.0);
        ApplyFog(position.xyz, final_result);
        result = final_result.rgb;

        // TEMP -- debugging
        // result = albedo.rgb;

#ifdef PATHTRACER
        result = CalculatePathTracing(deferred_params, texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
        result = ibl.rgb;
#elif defined(DEBUG_IRRADIANCE)
        result = irradiance.rgb;
#endif
    } else {
        result = albedo.rgb;
    }

    output_color = vec4(result, 1.0);
}