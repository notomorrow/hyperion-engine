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
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer, size = 147456) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe, size = 512) readonly buffer CurrentEnvProbe { EnvProbe current_env_probe; };

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "include/shared.inc"
#include "include/gbuffer.inc"
#include "include/material.inc"

#include "include/scene.inc"
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SSBO(Scene, ShadowMapsBuffer) readonly buffer ShadowMapsBuffer
{
    ShadowMap shadow_map_data[16];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, LightsBuffer) readonly buffer LightsBuffer
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

HYP_DESCRIPTOR_SAMPLER(DeferredDirectDescriptorSet, LTCSampler) uniform sampler ltc_sampler;
HYP_DESCRIPTOR_SRV(DeferredDirectDescriptorSet, LTCMatrixTexture) uniform texture2D ltc_matrix_texture;
HYP_DESCRIPTOR_SRV(DeferredDirectDescriptorSet, LTCBRDFTexture) uniform texture2D ltc_brdf_texture;

HYP_DESCRIPTOR_SSBO(DeferredDirectDescriptorSet, MaterialsBuffer) readonly buffer MaterialsBuffer
{
    Material materials[HYP_MAX_MATERIALS];
};

HYP_DESCRIPTOR_SRV(Material, Textures) uniform texture2D textures[];

#include "include/LightSampling.glsl"

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

    vec4 view_space_position = ReconstructViewSpacePositionFromDepth(inverse(camera.projection), texcoord, depth);
   
    vec4 position = inverse(camera.view) * view_space_position;
    position /= position.w;

    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = transmission, a = AO */

    const float roughness = material.r;
    const float metalness = material.g;

    bool perform_lighting = albedo.a > 0.0;
    vec4 result = vec4(0.0);

    if (perform_lighting)
    {
        const float material_reflectance = 0.5;
        // dialetric f0
        const float reflectance = 0.16 * material_reflectance * material_reflectance;
        vec4 F0 = vec4(albedo.rgb * metalness + (reflectance * (1.0 - metalness)), 1.0);

        const vec4 diffuse_color = CalculateDiffuseColor(albedo, metalness);

        vec4 F90 = vec4(clamp(dot(F0, vec4(50.0 * 0.33)), 0.0, 1.0));

        vec3 N = normalize(normal);
        vec3 T = normalize(tangent);
        vec3 B = normalize(bitangent);
        vec3 V = normalize(camera.position.xyz - position.xyz);

        const float NdotV = max(0.000001, dot(N, V));
    
        float ao = 1.0;
        float shadow = 1.0;

        const vec4 ssao_data = Texture2D(HYP_SAMPLER_NEAREST, ssao_gi_result, texcoord);
        ao = min(mix(1.0, ssao_data.a, bool(deferred_params.flags & DEFERRED_FLAGS_HBAO_ENABLED)), material.a);

        vec4 area_light_radiance;

#if defined(LIGHT_TYPE_DIRECTIONAL) || defined(LIGHT_TYPE_POINT) || defined(LIGHT_TYPE_SPOT)
        vec3 L = light.position_intensity.xyz;
        L -= position.xyz * float(min(light.type, 1));
        L = normalize(L);

        const vec3 H = normalize(L + V);

        const float NdotL = max(0.000001, dot(N, L));
        const float LdotH = max(0.000001, dot(L, H));
        const float NdotH = max(0.000001, dot(N, H));
        const float HdotV = max(0.000001, dot(H, V));
#elif defined(LIGHT_TYPE_AREA_RECT)
        vec3 L;

        const vec3 R = reflect(-V, N);

        // const float theta = acos(NdotV);
        // vec2 lut_uv = (vec2(roughness, theta / HYP_FMATH_PI * 0.5));
        vec2 lut_uv = (vec2(roughness, sqrt(1.0 - NdotV)));
        lut_uv.y = 1.0 - lut_uv.y;
        lut_uv = lut_uv * lut_scale + lut_bias;
        lut_uv = clamp(lut_uv, vec2(0.0), vec2(1.0));

        const vec4 t1 = Texture2D(ltc_sampler, ltc_matrix_texture, lut_uv);
        const vec4 t2 = Texture2D(ltc_sampler, ltc_brdf_texture, lut_uv);

        const mat3 Minv = mat3(
            vec3(t1.x, 0.0, t1.y),
            vec3(0.0, 1.0, 0.0),
            vec3(t1.z, 0.0, t1.w)
        );

        const float half_width = light.area_size.x * 0.5;
        const float half_height = light.area_size.y * 0.5;

        vec3 light_tangent;
        vec3 light_bitangent;
        ComputeOrthonormalBasis(light.normal.xyz, light_tangent, light_bitangent);
    
        const vec3 p0 = light.position_intensity.xyz - light_tangent * half_width - light_bitangent * half_height;
        const vec3 p1 = light.position_intensity.xyz + light_tangent * half_width - light_bitangent * half_height;
        const vec3 p2 = light.position_intensity.xyz + light_tangent * half_width + light_bitangent * half_height;
        const vec3 p3 = light.position_intensity.xyz - light_tangent * half_width + light_bitangent * half_height;

        const vec3 pts[4] = vec3[4](p0, p1, p2, p3);

        vec4 area_light_diffuse = CalculateAreaLightRadiance(light, mat3(1.0), pts, position.xyz, N, V);
        area_light_diffuse *= diffuse_color * (1.0 / HYP_FMATH_PI);

        vec4 area_light_specular = CalculateAreaLightRadiance(light, Minv, pts, position.xyz, N, V);

        // GGX BRDF shadowing and Fresnel
        // t2.x: shadowedF90 (F90 normally it should be 1.0)
        // t2.y: Smith function for Geometric Attenuation Term, it is dot(V or L, H).
        area_light_specular *= diffuse_color * t2.x + (vec4(1.0) - diffuse_color) * t2.y;
        area_light_radiance = area_light_specular + area_light_diffuse;

        const float NdotL = 0.0;
        const float LdotH = 0.0;
        const float NdotH = 0.0;
        const float HdotV = 0.0;
#else
        const float NdotL = 0.0;
        const float LdotH = 0.0;
        const float NdotH = 0.0;
        const float HdotV = 0.0;
#endif

        vec4 light_rays = vec4(0.0);
        vec4 light_color = UINT_TO_VEC4(light.color_encoded);

#ifdef LIGHT_TYPE_POINT
        if (light.shadow_map_index != ~0u && current_env_probe.texture_index != ~0u) {
            const vec3 world_to_light = position.xyz - light.position_intensity.xyz;
            const uint shadow_flags = current_env_probe.flags >> 3;

            shadow = GetPointShadow(current_env_probe.texture_index, shadow_flags, world_to_light);
        }
#elif defined(LIGHT_TYPE_DIRECTIONAL)
        if (light.shadow_map_index != ~0u) {
            shadow = GetShadow(light.shadow_map_index, position.xyz, texcoord, camera.dimensions.xy, NdotL);

// #ifdef LIGHT_RAYS_ENABLED
//             const float linear_eye_depth = ViewDepth(depth, camera.near, camera.far);

//             const float light_ray_attenuation = LightRays(
//                 light.shadow_map_index,
//                 texcoord,
//                 L,
//                 position.xyz,
//                 camera.position.xyz,
//                 depth
//             );

//             light_rays = vec4(light_color * light_ray_attenuation);
// #endif
        }
#endif

        const float D = CalculateDistributionTerm(roughness, NdotH);
        const float G = CalculateGeometryTerm(NdotL, NdotV, HdotV, NdotH);
        const vec4 F = CalculateFresnelTerm(F0, roughness, LdotH);

        const vec4 dfg = CalculateDFG(F, roughness, NdotV);
        const vec4 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0.rgb, dfg.rgb);

        const vec4 specular_lobe = D * G * F;

#if defined(LIGHT_TYPE_POINT) || defined(LIGHT_TYPE_SPOT)
        float attenuation = GetSquareFalloffAttenuation(position.xyz, light.position_intensity.xyz, light.radius);

#ifdef LIGHT_TYPE_SPOT
        float theta = max(dot(-L, normalize(light.normal.xyz)), 0.0);

        attenuation *= saturate((theta - light.spot_angles[0]) / (light.spot_angles[1] - light.spot_angles[0])) * step(light.spot_angles.x, theta);
#endif

#else
        const float attenuation = 1.0;
#endif

        vec4 specular = specular_lobe;

        vec4 diffuse_lobe = diffuse_color * (1.0 / HYP_FMATH_PI);
        vec4 diffuse = diffuse_lobe;

        vec4 direct_component = diffuse + specular * vec4(energy_compensation, 1.0);

        result += direct_component * (light_color * ao * NdotL * shadow * light.position_intensity.w * attenuation);
        result.a = attenuation;

        // result = vec4(vec3(1.0 / max(dfg.y, 0.0001)), 1.0);
#ifdef LIGHT_TYPE_AREA_RECT
        // debugging area lights
        result = area_light_radiance;
#endif

        result = (result * (1.0 - light_rays.a)) + light_rays;
    }

#if defined(DEBUG_REFLECTIONS) || defined(DEBUG_IRRADIANCE) || defined(PATHTRACER)
    output_color = vec4(0.0);
#else
    output_color = vec4(result);
#endif
}
