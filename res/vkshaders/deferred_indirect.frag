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

#ifdef VCT_ENABLED_TEXTURE
    #include "include/vct/cone_trace.inc"
#endif

/* Begin main shader program */

#define IBL_INTENSITY 7500.0
#define IRRADIANCE_MULTIPLIER 1.0

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
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    
    float ao = 1.0;
    vec3 irradiance = vec3(0.0);
    vec4 reflections = vec4(0.0);
    vec3 ibl = vec3(0.0);
    vec3 F = vec3(0.0);

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi_result, v_texcoord0);
    ao = min(bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED) ? ssao_data.a : 1.0, material.a);
    
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
        const vec3 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

#ifdef VCT_ENABLED_TEXTURE
        if (IsRenderComponentEnabled(HYP_RENDER_COMPONENT_VCT)) {
            vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
            vct_diffuse = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

#if HYP_VCT_INDIRECT_ENABLED
            irradiance = vct_diffuse.rgb;
#endif

#if HYP_VCT_REFLECTIONS_ENABLED
            reflections = vct_specular;
#endif
        }
#endif

#ifdef ENV_PROBE_ENABLED
        ibl = CalculateEnvProbeReflection(deferred_params, position.xyz, N, R, scene.camera_position.xyz, perceptual_roughness);
#endif

#ifdef SSR_ENABLED
        CalculateScreenSpaceReflection(deferred_params, texcoord, depth, reflections);
#endif

#ifdef ENV_PROBE_ENABLED
        // CalculateEnvProbeIrradiance(deferred_params, position.xyz, N, irradiance);
#endif

#ifdef RT_ENABLED
        CalculateRaytracingReflection(deferred_params, texcoord, reflections);

        irradiance += DDGISampleIrradiance(position.xyz, N, V).rgb;
#endif

        CalculateHBILIrradiance(deferred_params, ssao_data, irradiance);

        vec3 Fd = diffuse_color.rgb * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - E) * ao;

        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        specular_ao *= energy_compensation;

        vec3 Fr = E * ibl;

        Fr *= specular_ao;
        reflections.rgb *= specular_ao;

        vec3 multibounce = GTAOMultiBounce(ao, albedo.rgb);
        Fd *= multibounce;
        Fr *= multibounce;

        Fr *= exposure * IBL_INTENSITY;
        Fd *= exposure * IBL_INTENSITY;

        IntegrateReflections(Fr, E, reflections);

        result = Fd + Fr;

        vec4 final_result = vec4(result, 1.0);
        ApplyFog(position.xyz, final_result);
        result = final_result.rgb;
    } else {
        result = albedo.rgb;
    }

    output_color = vec4(result.rgb, 1.0);
}