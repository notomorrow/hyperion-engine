#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/PostFXSample.inc"
#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;

#include "include/shadows.inc"
#include "include/PhysicalCamera.inc"

void main()
{
    vec4 albedo    = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal    = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(SampleGBuffer(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    float depth    = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position  = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection * scene.view), texcoord, depth);
    vec4 material  = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;

    const float material_reflectance = 0.5;
    const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
    const vec4 F0 = albedo * metalness + (reflectance * (1.0 - metalness));


    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
    
    const vec2 AB = BRDFMap(roughness, NdotV);
    
    const vec4 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
    const float perceptual_roughness = sqrt(roughness);
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;
        // surface

        float shadow = 1.0;

        vec3 L = light.position.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        vec3 H = normalize(L + V);

        float NdotL = max(0.0001, dot(N, L));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));

        if (light.shadow_map_index != ~0u) {
#if HYP_SHADOW_PENUMBRA
            shadow = GetShadowContactHardened(light.shadow_map_index, position.xyz, NdotL);
#else
            shadow = GetShadow(light.shadow_map_index, position.xyz, vec2(0.0), NdotL);
#endif
        }

        vec4 light_color = unpackUnorm4x8(light.color_encoded);

        vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));
        float D = DistributionGGX(N, H, roughness);
        float G = V_SmithGGXCorrelated(roughness, NdotV, NdotL);//CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        vec4 F = SchlickFresnel(F0, F90, LdotH);

        const vec4 diffuse_color = (albedo) * (1.0 - metalness);
        vec4 specular = D * G * F;

        float dist = max(length(light.position.xyz - position.xyz), light.radius);
        float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
            (light.intensity / max(dist * dist, 0.0001)) : 1.0;

        vec4 light_scatter = SchlickFresnel(vec4(1.0), F90, NdotL);
        vec4 view_scatter  = SchlickFresnel(vec4(1.0), F90, NdotV);
        vec4 diffuse = diffuse_color * (1.0 / HYP_FMATH_PI);///(light_scatter * view_scatter * (1.0 / PI));
        vec4 direct_component = (specular + diffuse * energy_compensation) * (light.intensity */*(exposure * light.intensity) **/ (attenuation * NdotL) * ao * shadow * light_color);

        direct_component = CalculateFogLinear(direct_component, vec4(0.7, 0.8, 1.0, 1.0), position.xyz, scene.camera_position.xyz, (scene.camera_near + scene.camera_far) * 0.5, scene.camera_far);

        result += direct_component;
    } else {
        result = albedo;
    }

    output_color = result;
}
