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
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 58) uniform texture2D reflection_probes_texture;

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

#if defined(VCT_ENABLED_TEXTURE) || defined(VCT_ENABLED_SVO)
    #include "include/vct/cone_trace.inc"
#endif

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

#include "include/DDGI.inc"

void main()
{
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
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
    
#if defined(VCT_ENABLED_TEXTURE) || defined(VCT_ENABLED_SVO)
    vec4 vct_specular = vec4(0.0);
    vec4 vct_diffuse = vec4(0.0);
#endif

    if (perform_lighting) {
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

#if defined(VCT_ENABLED_TEXTURE) || defined(VCT_ENABLED_SVO)
#ifdef VCT_ENABLED_TEXTURE
        #define VCT_RENDER_COMPONENT_INDEX HYP_RENDER_COMPONENT_VCT
#else
        #define VCT_RENDER_COMPONENT_INDEX HYP_RENDER_COMPONENT_SVO
#endif

        // if (IsRenderComponentEnabled(VCT_RENDER_COMPONENT_INDEX)) {
            vct_specular = ConeTraceSpecular(position.xyz, N, R, perceptual_roughness);
            vct_diffuse = ConeTraceDiffuse(position.xyz, N, T, B, perceptual_roughness);

#if HYP_VCT_INDIRECT_ENABLED
            irradiance = vct_diffuse.rgb;
#endif

#if HYP_VCT_REFLECTIONS_ENABLED
            reflections = vct_specular;
#endif
        // }
#endif

#ifdef REFLECTION_PROBE_ENABLED
        vec4 reflection_probes_color = Texture2D(HYP_SAMPLER_LINEAR, reflection_probes_texture, texcoord);
        ibl.rgb = reflection_probes_color.rgb * reflection_probes_color.a;
#endif

#ifdef SSR_ENABLED
        CalculateScreenSpaceReflection(deferred_params, texcoord, depth, reflections);
#endif

        irradiance += Texture2D(HYP_SAMPLER_LINEAR, env_grid_irradiance_texture, texcoord).rgb * ENV_PROBE_MULTIPLIER;

#ifdef RT_REFLECTIONS_ENABLED
        CalculateRaytracingReflection(deferred_params, texcoord, reflections);
#endif

//#ifdef RT_GI_ENABLED
        irradiance += DDGISampleIrradiance(position.xyz, N, V).rgb;
//#endif

#ifdef HBIL_ENABLED
        CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);
#endif

        vec3 Fd = diffuse_color.rgb * irradiance * (1.0 - E) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        specular_ao *= energy_compensation;

        vec3 reflection_combined = (ibl.rgb * (1.0 - reflections.a)) + (reflections.rgb);
        vec3 Fr = reflection_combined * E * specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
        Fd *= multibounce;
        Fr *= multibounce;

        // Fr *= exposure * IBL_INTENSITY;
        // Fd *= exposure * IBL_INTENSITY;

        // IntegrateReflections(Fr, reflections);

        result = Fd + Fr;

        vec4 final_result = vec4(result, 1.0);
        ApplyFog(position.xyz, final_result);
        result = final_result.rgb;

#ifdef PATHTRACER
        result = CalculatePathTracing(deferred_params, texcoord).rgb;
#elif defined(DEBUG_REFLECTIONS)
        result = reflection_combined.rgb;
#elif defined(DEBUG_IRRADIANCE)
        result = irradiance.rgb;
#endif
    } else {
        result = albedo.rgb;
    }

    output_color = vec4(result, 1.0);
}