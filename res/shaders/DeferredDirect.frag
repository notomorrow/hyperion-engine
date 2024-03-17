#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(Global, SSAOResultTexture) uniform texture2D ssao_gi_result;
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

#include "include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform textureCube env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer, size = 4352) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, size = 147456) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};

#include "include/shared.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"

#include "include/scene.inc"
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

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, LightsBuffer, size = 64) readonly buffer LightsBuffer
{
    Light light;
};

HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, count = 16) uniform texture2D shadow_maps[16];
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapTextures, count = 16) uniform textureCube point_shadow_maps[16];

#define HYP_DEFERRED_NO_REFRACTION
#include "./deferred/DeferredLighting.glsl"
#undef HYP_DEFERRED_NO_REFRACTION

vec2 texcoord = v_texcoord0;

#include "include/shadows.inc"

#include "include/PhysicalCamera.inc"
#include "include/LightRays.glsl"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(push_constant) uniform PushConstant
{
    DeferredParams deferred_params;
};

void main()
{
    vec4 albedo = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture, texcoord);
    uint mask = VEC4_TO_UINT(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord));
    vec3 normal = DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(normal, tangent, bitangent);

    float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    vec4 position = ReconstructWorldSpacePositionFromDepth(inverse(camera.projection), inverse(camera.view), texcoord, depth);
    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = transmission, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;

    const float material_reflectance = 0.5;
    // dialetric f0
    const float reflectance = 0.16 * material_reflectance * material_reflectance;
    vec4 F0 = vec4(albedo.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);

    vec3 N = normalize(normal);
    vec3 T = normalize(tangent);
    vec3 B = normalize(bitangent);
    vec3 V = normalize(camera.position.xyz - position.xyz);

    float NdotV = max(0.0001, dot(N, V));
 
    vec4 result = vec4(0.0);
    
    float ao = 1.0;
    float shadow = 1.0;

    const vec4 ssao_data = Texture2D(HYP_SAMPLER_NEAREST, ssao_gi_result, texcoord);
    ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);

    vec3 L = light.position_intensity.xyz;
    L -= position.xyz * float(min(light.type, 1));
    L = normalize(L);

    const vec3 H = normalize(L + V);

    const float NdotL = max(0.0001, dot(N, L));
    const float NdotH = max(0.0001, dot(N, H));
    const float LdotH = max(0.0001, dot(L, H));
    const float HdotV = max(0.0001, dot(H, V));

    vec4 light_rays = vec4(0.0);
    vec4 light_color = unpackUnorm4x8(light.color_encoded);

    if (light.type == HYP_LIGHT_TYPE_POINT && light.shadow_map_index != ~0u && current_env_probe.texture_index != ~0u) {
        const vec3 world_to_light = position.xyz - light.position_intensity.xyz;
        const uint shadow_flags = current_env_probe.flags >> 3;

        shadow = GetPointShadow(current_env_probe.texture_index, shadow_flags, world_to_light);
    } else if (light.type == HYP_LIGHT_TYPE_DIRECTIONAL && light.shadow_map_index != ~0u) {
        shadow = GetShadow(light.shadow_map_index, position.xyz, texcoord, camera.dimensions.xy, NdotL);

#ifdef LIGHT_RAYS_ENABLED
        const float linear_eye_depth = ViewDepth(depth, camera.near, camera.far);

        const float light_ray_attenuation = LightRays(
            light.shadow_map_index,
            texcoord,
            L,
            position.xyz,
            camera.position.xyz,
            depth
        );

        light_rays = vec4(light_color * light_ray_attenuation);
#endif
    }

    if (perform_lighting && !bool(mask & 0x10)) {
        vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));
        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const vec4 F = CalculateFresnelTerm(F0, roughness, LdotH);

        //const float perceptual_roughness = sqrt(roughness);
        const vec4 dfg = CalculateDFG(F, roughness, NdotV);
        const vec4 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

        const vec4 diffuse_color = CalculateDiffuseColor(albedo, metalness);
        const vec4 specular_lobe = D * G * F;

        const float attenuation = light.type == HYP_LIGHT_TYPE_POINT
            ? GetSquareFalloffAttenuation(position.xyz, light.position_intensity.xyz, light.radius)
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

    result = (result * (1.0 - light_rays.a)) + light_rays;

#if defined(DEBUG_REFLECTIONS) || defined(DEBUG_IRRADIANCE) || defined(PATHTRACER)
    output_color = vec4(0.0);
#else
    output_color = vec4(result);
#endif

    // output_color = vec4(Texture2D(HYP_SAMPLER_LINEAR, shadow_maps[0], texcoord).rg, 0.0, 1.0);
}
