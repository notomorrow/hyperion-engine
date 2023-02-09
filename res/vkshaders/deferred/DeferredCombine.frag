#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 color_output;

// #define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/shared.inc"
#include "../include/defines.inc"
#include "../include/gbuffer.inc"
#include "../include/scene.inc"
#include "../include/brdf.inc"
#include "../include/env_probe.inc"
// #undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 45) uniform texture2D rt_radiance_final;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 55) uniform texture2D deferred_indirect_lighting;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 56) uniform texture2D deferred_direct_lighting;

#include "./DeferredLighting.glsl"
#include "../include/shadows.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
// #define HYP_DEFERRED_NO_ENV_PROBE // temp

layout(push_constant) uniform DeferredCombineData
{
    uvec2 image_dimensions;
    uint _pad0;
    uint _pad1;
    DeferredParams deferred_params;
};

#if 0
struct Refraction
{
    vec3 position;
    vec3 direction;
};

float ApplyIORToRoughness(float ior, float roughness)
{
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}

void RefractionSolidSphere(
    vec3 P, vec3 N, vec3 V, float eta_ir,
    out Refraction out_refraction
)
{
    const float thickness = 0.8;

    const vec3 R = refract(-V, N, eta_ir);
    float NdotR = dot(N, R);
    float d = thickness * -NdotR;

    Refraction refraction;
    refraction.position = vec3(P + R * d);
    vec3 n1 = normalize(NdotR * R - N * 0.5);
    refraction.direction = refract(R, n1, eta_ir);

    out_refraction = refraction;
}
#endif

void main()
{
    vec4 indirect_lighting = Texture2D(HYP_SAMPLER_NEAREST, deferred_indirect_lighting, texcoord);
    indirect_lighting.a = 1.0;

    vec4 forward_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture_translucent, texcoord);
    vec3 albedo = forward_result.rgb;

    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord);
    const float roughness = material.r;
    const float metalness = material.g;
    const float transmission = material.b;
    const float ao = material.a;
    const float perceptual_roughness = sqrt(roughness);

    const vec3 N = DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    const float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    const vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);
    const vec3 P = position.xyz;
    const vec3 V = normalize(camera.position.xyz - position.xyz);
    const vec3 R = normalize(reflect(-V, N));

    const uint object_mask = VEC4_TO_UINT(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord));

    vec3 L = light.position_intensity.xyz;
    L -= position.xyz * float(min(light.type, 1));
    L = normalize(L);

    const vec3 H = normalize(L + V);
    const float NdotL = max(0.0001, dot(N, L));
    const float NdotH = max(0.0001, dot(N, H));
    const float LdotH = max(0.0001, dot(L, H));
    const float HdotV = max(0.0001, dot(H, V));

    // vec4 result = vec4(
    //     (direct_lighting.rgb * direct_lighting.a) + (indirect_lighting.rgb * (1.0 - direct_lighting.a)),
    //     1.0
    // );

    vec4 result = indirect_lighting;

    vec4 forward_lit_result = vec4(0.0, 0.0, 0.0, forward_result.a);

    if (bool(object_mask & 0x02)) {
#if 0
        const float NdotV = max(HYP_FMATH_EPSILON, dot(N, V));
        const vec3 F0 = CalculateF0(forward_result.rgb, metalness);
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));

        vec3 Ft = vec3(0.0);
        vec3 Fr = vec3(0.0);
        vec3 Fd = vec3(0.0);

        vec3 irradiance = vec3(0.0);
        vec4 ibl = vec4(0.0);
        vec4 reflections = vec4(0.0);

        { // irradiance
            const vec3 F = CalculateFresnelTerm(F0, roughness, NdotV);

            const vec3 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            Ft = CalculateRefraction(
                image_dimensions,
                P, N, V,
                texcoord,
                F0, E,
                transmission, roughness,
                result, forward_result,
                vec3(ao)
            );

    #ifdef ENV_GRID_ENABLED
            CalculateEnvProbeIrradiance(P, N, irradiance);
    #endif

            Fd = forward_result.rgb * irradiance * (1.0 - E) * ao;

    #ifdef REFLECTION_PROBE_ENABLED
            // ibl = CalculateEnvProbeReflection(deferred_params, position.xyz, N, R, perceptual_roughness);
            ibl = CalculateReflectionProbe(current_env_probe, P, N, R, camera.position.xyz, perceptual_roughness);
            ibl.a = saturate(ibl.a);
    #endif

    #ifdef SSR_ENABLED
            CalculateScreenSpaceReflection(deferred_params, texcoord, depth, reflections);
    #endif

            vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
            specular_ao *= energy_compensation;

            vec3 reflection_combined = (ibl.rgb * (1.0 - reflections.a)) + (reflections.rgb);
            Fr = reflection_combined * E * specular_ao;
        }

        Ft *= transmission;
        Fd *= (1.0 - transmission);

        forward_lit_result.rgb += Ft + Fd + Fr;

        Fr = vec3(0.0);

        { // direct lighting
            float shadow = 1.0;

            if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
                shadow = GetShadow(light.shadow_map_index, P, texcoord, NdotL);
            }

            vec3 light_color = UINT_TO_VEC4(light.color_encoded).rgb;

            const float D = CalculateDistributionTerm(roughness, NdotH);
            const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
            const vec3 F = CalculateFresnelTerm(F0, roughness, LdotH);

            const vec3 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
            const vec3 E = CalculateE(F0, dfg);
            const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);

            const vec3 diffuse_color = CalculateDiffuseColor(albedo, metalness);
            const vec3 specular_lobe = D * G * F;

            vec3 specular = specular_lobe;

            const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
                ? GetSquareFalloffAttenuation(position.xyz, V, light.position_intensity.xyz, light.radius)
                : 1.0;

            vec3 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
            vec3 diffuse = diffuse_lobe;
            diffuse *= (1.0 - transmission);

            vec3 direct_component = diffuse + specular * energy_compensation;

            // direct_component.rgb *= (exposure);
            Fr += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        }

        forward_lit_result.rgb += Fr;
#endif
        
        // result.rgb = (forward_lit_result.rgb) + (result.rgb * (1.0 - forward_lit_result.a));
        result.rgb = (forward_result.rgb * forward_result.a) + (result.rgb * (1.0 - forward_result.a));
    } else {
        result.rgb += forward_result.rgb * forward_result.a;
    }

    // result.rgb = forward_result.aaa;

    // result.rgb = (forward_result.rgb * forward_result.a) + (result.rgb * (1.0 - forward_result.a));

    // result.rgb = forward_result.aaa;
    // result.rgb = forward_lit_result.rgb;
    // result.rgb = vec3(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord).rgb);//  DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    result.a = 1.0;

    color_output = result;
}