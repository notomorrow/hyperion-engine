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
    vec4 albedo = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);

    vec4 tangents_buffer = SampleGBuffer(gbuffer_tangents_texture, texcoord);

    vec3 tangent = UnpackNormalVec2(tangents_buffer.xy);
    vec3 bitangent = UnpackNormalVec2(tangents_buffer.zw);

    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(scene.projection), inverse(scene.view), texcoord, depth);
    vec4 material = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;

    // const float material_reflectance = 0.5;
    // const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
    
    
    // Index of refraction for common dielectrics. Corresponds to f0 4%
    const float IOR = 1.5;

    // Reflectance of the surface when looking straight at it along the negative normal
    const vec4 reflectance = vec4(pow(IOR - 1.0, 2.0) / pow(IOR + 1.0, 2.0));
    vec4 F0 = albedo * metalness + (reflectance * (1.0 - metalness));

    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;
        // surface

        float shadow = 1.0;

        vec3 L = light.position.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);

        const float NdotL = max(0.0001, dot(N, L));
        const float NdotH = max(0.0001, dot(N, H));
        const float LdotH = max(0.0001, dot(L, H));
        const float VdotH = max(0.0001, dot(V, H));

        if (light.shadow_map_index != ~0u) {
            shadow = GetShadow(light.shadow_map_index, position.xyz, NdotL);
        }

        vec4 light_color = unpackUnorm4x8(light.color_encoded);

        vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));
        const float D = DistributionGGX(N, H, roughness);
        const float G = V_SmithGGXCorrelated(roughness, NdotV, NdotL);//CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        const vec4 F = SchlickFresnelRoughness(F0, roughness, LdotH);//SchlickFresnelRoughness(F0.rgb, roughness, LdotH), albedo.a);

        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec4 dfg = F * AB.x + AB.y;
        const vec4 energy_compensation = 1.0 + F0 * ((1.0 / max(dfg.y, 0.00001)) - 1.0);

        // const vec4 diffuse_color = (albedo) * (1.0 - metalness);
        const vec4 specular_lobe = D * G * F;

        float dist = max(length(light.position.xyz - position.xyz), light.radius);
        float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
            (light.intensity / max(dist * dist, 0.0001)) : 1.0;

        const vec4 light_scatter = SchlickFresnel(vec4(1.0), F90, NdotL);
        const vec4 view_scatter  = SchlickFresnel(vec4(1.0), F90, NdotV);
        
        const vec4 kD = (1.0 - F) * (1.0 - metalness);

        vec4 specular = specular_lobe;

        vec4 diffuse_lobe = albedo * (light_scatter * view_scatter) / HYP_FMATH_PI;
        vec4 diffuse = diffuse_lobe;

        vec4 direct_component = kD * diffuse + specular * energy_compensation;

        
        direct_component = CalculateFogLinear(direct_component, vec4(0.7, 0.8, 1.0, 1.0), position.xyz, scene.camera_position.xyz, (scene.camera_near + scene.camera_far) * 0.5, scene.camera_far);
        result += direct_component * ((exposure * light.intensity) * light_color * ao * NdotL * shadow * attenuation);

    } else {
        result = albedo;
    }

    output_color = result;
}
