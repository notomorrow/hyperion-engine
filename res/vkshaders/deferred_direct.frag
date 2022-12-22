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

void main()
{
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);

    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = transmission, a = AO */

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
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    float shadow = 1.0;

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi_result, texcoord);
    ao = min(bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED) ? ssao_data.a : 1.0, material.a);

    if (perform_lighting) {

        vec3 L = light.position_intensity.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);

        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float HdotV = max(0.0001, dot(H, V));

        if (light.shadow_map_index != ~0u) {
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

        const float dist = length(light.position_intensity.xyz - position.xyz);
        const float r = max(light.radius, HYP_FMATH_EPSILON);
        const float d = max(dist - r, 0.0);
        const float denom = 1.0 + (d / r);
        const float cutoff = 0.001;
    
        float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
            ((1.0 / (max(HYP_FMATH_SQR(denom), HYP_FMATH_EPSILON)))) : 1.0;

        attenuation = mix(1.0, (attenuation - cutoff) / (1.0 - cutoff), light.type == HYP_LIGHT_TYPE_POINT);
        attenuation = saturate(attenuation);
    
        // attenuation *= attenuation;
        // attenuation = Saturate(attenuation);
        // float attenuation = 1.0;//mix(1.0, 1.0 - min(length(light.position_intensity.xyz - position.xyz) / max(light.radius, 0.0001), 1.0), float(light.type == HYP_LIGHT_TYPE_POINT));

        vec4 specular = specular_lobe;

        vec4 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        vec4 diffuse = diffuse_lobe;

        vec4 direct_component = diffuse + specular;// * vec4(energy_compensation, 1.0);

        direct_component.rgb *= (exposure);
        result += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        result.a = attenuation;

        // ApplyFog(position.xyz, result);


        // result = vec4(vec3(1.0 / max(dfg.y, 0.0001)), 1.0);
    } else {
        result = albedo;
    }

    output_color = vec4(result);
}
