#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 41) uniform texture2D ssao_gi_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 45) uniform texture2D rt_radiance_final;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/PostFXSample.inc"
#include "include/scene.inc"

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#include "include/shadows.inc"
#include "include/PhysicalCamera.inc"

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

// Used for point light shadows
layout(std140, set = HYP_DESCRIPTOR_SET_SCENE, binding = 3, row_major) readonly buffer EnvProbeBuffer
{
    EnvProbe current_env_probe;
};

void main()
{
    vec4 albedo = Texture2D(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);

    float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);
    vec4 material = Texture2D(HYP_SAMPLER_LINEAR, gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = transmission, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;

    const float material_reflectance = 0.5;
    // dialetric f0
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    vec4 F0 = vec4(albedo.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(camera.position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    float shadow = 1.0;

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi_result, texcoord);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);

    if (perform_lighting) {

        vec3 L = light.position_intensity.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);

        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        if (light.type == HYP_LIGHT_TYPE_POINT && light.shadow_map_index != ~0u && current_env_probe.texture_index != ~0u) {
            const vec3 world_to_light = position.xyz - light.position_intensity.xyz;
            const uint shadow_flags = current_env_probe.flags >> 3;

            shadow = GetPointShadow(current_env_probe.texture_index, shadow_flags, world_to_light);
        } else if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
            shadow = GetShadow(light.shadow_map_index, position.xyz, texcoord, NdotL);
        }

        vec4 light_color = unpackUnorm4x8(light.color_encoded);

        vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));
        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const vec4 F = CalculateFresnelTerm(F0, roughness, LdotH);

        const float perceptual_roughness = sqrt(roughness);
        const vec4 dfg = CalculateDFG(F, perceptual_roughness, NdotV);
        const vec4 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

        const vec4 diffuse_color = CalculateDiffuseColor(albedo, metalness);
        const vec4 specular_lobe = D * G * F;

        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(position.xyz, V, light.position_intensity.xyz, light.radius)
            : 1.0;

        vec4 specular = specular_lobe;

        vec4 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        vec4 diffuse = diffuse_lobe;

        vec4 direct_component = diffuse + specular * vec4(energy_compensation, 1.0);

        // direct_component.rgb *= (exposure);
        result += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        result.a = attenuation;

        // ApplyFog(position.xyz, result);

        // result = vec4(vec3(1.0 / max(dfg.y, 0.0001)), 1.0);
    } else {
        result = albedo;
    }

#if defined(DEBUG_REFLECTIONS) || defined(DEBUG_IRRADIANCE)
    output_color = vec4(0.0);
#else
    output_color = vec4(result);
#endif
}
