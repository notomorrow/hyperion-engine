#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 12) uniform texture2D ssr_uvs;
// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 13) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;

#include "include/env_probe.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/post_fx.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);

#include "include/shadows.inc"

/* Begin main shader program */

#define IBL_INTENSITY 10000.0
#define DIRECTIONAL_LIGHT_INTENSITY 100000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define ROUGHNESS_LOD_MULTIPLIER 12.0
#define SSAO_DEBUG 0

#include "include/rt/probe/shared.inc"

void main()
{
    vec4 albedo    = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal    = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(SampleGBuffer(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    float depth    = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position  = ReconstructPositionFromDepth(inverse(scene.projection * scene.view), texcoord, depth);
    vec4 material  = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;    

    bool perform_lighting = albedo.a > 0.0;
    
    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));

    const vec4 diffuse_color = albedo * (1.0 - metalness);

    const float material_reflectance = 0.5;
    const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
    const vec4 F0 = albedo * metalness + (reflectance * (1.0 - metalness));


    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
    
    const vec2 AB = BRDFMap(roughness, NdotV);
    const vec4 dfg = albedo * AB.x + AB.y;
    
    const vec4 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
    const float perceptual_roughness = sqrt(roughness);
    const float lod = ROUGHNESS_LOD_MULTIPLIER * perceptual_roughness * (2.0 - perceptual_roughness);
    
    vec4 result = vec4(0.0);

    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;
        // surface

        float shadow = 1.0;

        for (int i = 0; i < min(scene.num_lights, 8); i++) {
            Light light = lights[i];

            vec3 L = light.position.xyz;
            L -= position.xyz * float(min(light.type, 1));
            L = normalize(L);

            vec3 H = normalize(L + V);

            float NdotL = max(0.0001, dot(N, L));
            float NdotH = max(0.0001, dot(N, H));
            float LdotH = max(0.0001, dot(L, H));

            //! TODO: Shadow needs to be moved into a post effect?
            if (scene.num_environment_shadow_maps != 0) {
#if HYP_SHADOW_PENUMBRA
                shadow = GetShadowContactHardened(0, position.xyz, NdotL);
#else
                shadow = GetShadow(0, position.xyz, vec2(0.0), NdotL);
#endif
            }

            vec4 light_color = unpackUnorm4x8(light.color_encoded);

            vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));
            float D = DistributionGGX(N, H, roughness);
            float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
            vec4 F = SchlickFresnel(F0, F90, LdotH);
            vec4 specular = D * G * F;

            float dist = max(length(light.position.xyz - position.xyz), light.radius);
            float attenuation = (light.type == HYP_LIGHT_TYPE_POINT) ?
                (light.intensity/(dist*dist)) : 1.0;

            vec4 light_scatter = SchlickFresnel(vec4(1.0), F90, NdotL);
            vec4 view_scatter  = SchlickFresnel(vec4(1.0), F90, NdotV);
            vec4 diffuse = diffuse_color * (light_scatter * view_scatter * (1.0 / PI));
            result += (specular + diffuse * energy_compensation) * ((exposure * light.intensity) * NdotL * ao * shadow * light_color) * attenuation;
        }
    } else {
        result = albedo;
    }

    output_color = result;
}
