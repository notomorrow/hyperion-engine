#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 texcoord;
layout(location=0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
#else
HYP_DESCRIPTOR_SRV(Global, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferMaterialTexture) uniform texture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferLightmapTexture) uniform texture2D gbuffer_albedo_lightmap_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferMaskTexture) uniform texture2D gbuffer_mask_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferWSNormalsTexture) uniform texture2D gbuffer_ws_normals_texture;
HYP_DESCRIPTOR_SRV(Global, GBufferTranslucentTexture) uniform texture2D gbuffer_albedo_texture_translucent;
#endif

HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(Global, SSAOResultTexture) uniform texture2D ssao_gi_result;
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;
HYP_DESCRIPTOR_SRV(Global, DeferredIndirectResultTexture) uniform texture2D deferred_indirect_lighting;
HYP_DESCRIPTOR_SRV(Global, DeferredDirectResultTexture) uniform texture2D deferred_direct_lighting;

#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/object.inc"

#include "../include/scene.inc"
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

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentLight) readonly buffer CurrentLight
{
    Light light;
};

HYP_DESCRIPTOR_SRV(Scene, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Scene, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#include "../include/brdf.inc"

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[]; };
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, EnvGridsBuffer) uniform EnvGridsBuffer { EnvGrid env_grid; };
HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, CurrentEnvProbe) readonly buffer CurrentEnvProbe { EnvProbe current_env_probe; };
HYP_DESCRIPTOR_SRV(Global, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "./DeferredLighting.glsl"
#include "../include/shadows.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
// #define HYP_DEFERRED_NO_ENV_PROBE // temp

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

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
    vec4 deferred_result = Texture2D(HYP_SAMPLER_NEAREST, deferred_indirect_lighting, texcoord);
    deferred_result.a = 1.0;

    vec4 translucent_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture_translucent, texcoord);
    vec4 gbuffer_albedo = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture, texcoord);

    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture, texcoord);
    const float roughness = material.r;
    const float metalness = material.g;
    const float transmission = material.b;
    const float ao = material.a;
    const float perceptual_roughness = sqrt(roughness);

    const mat4 inverse_proj = inverse(camera.projection);
    const mat4 inverse_view = inverse(camera.view);

    const vec3 N = DecodeNormal(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_normals_texture, texcoord));
    const float depth = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, texcoord).r;
    const vec3 P = ReconstructWorldSpacePositionFromDepth(inverse_proj, inverse_view, texcoord, depth).xyz;
    const vec3 V = normalize(camera.position.xyz - P);
    const vec3 R = normalize(reflect(-V, N));

    const uint object_mask = VEC4_TO_UINT(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, texcoord));

    vec3 L = light.position_intensity.xyz;
    L -= P * float(min(light.type, 1));
    L = normalize(L);

    const vec3 H = normalize(L + V);
    const float NdotL = max(0.0001, dot(N, L));
    const float NdotH = max(0.0001, dot(N, H));
    const float LdotH = max(0.0001, dot(L, H));
    const float HdotV = max(0.0001, dot(H, V));

    // apply reflections to lightmapped objects
    if (bool(object_mask & OBJECT_MASK_LIGHTMAP)) {
        vec3 ibl = vec3(0.0);
        vec3 F = vec3(0.0);

        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = CalculateDiffuseColor(gbuffer_albedo.rgb, metalness);
        const vec3 F0 = CalculateF0(gbuffer_albedo.rgb, metalness);

        F = CalculateFresnelTerm(F0, roughness, NdotV);
        const vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

        const float perceptual_roughness = sqrt(roughness);
        const vec3 dfg = CalculateDFG(F, roughness, NdotV);
        const vec3 E = CalculateE(F0, dfg);
        const vec3 energy_compensation = CalculateEnergyCompensation(F0, dfg);
    
        vec4 reflections_color = Texture2D(HYP_SAMPLER_NEAREST, reflections_texture, texcoord);
        ibl = ibl * (1.0 - reflections_color.a) + (reflections_color.rgb * reflections_color.a);

        vec4 rt_radiance = Texture2D(HYP_SAMPLER_NEAREST, rt_radiance_final, texcoord);
        ibl = ibl * (1.0 - rt_radiance.a) + (rt_radiance.rgb * rt_radiance.a);

        vec3 spec = (ibl * mix(dfg.xxx, dfg.yyy, F0)) * energy_compensation;

        vec4 gbuffer_albedo_lightmap = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_lightmap_texture, texcoord);

        deferred_result = (gbuffer_albedo * gbuffer_albedo_lightmap) + vec4(spec, 0.0);
    }

    vec4 result = mix(deferred_result, translucent_result, bvec4(bool(object_mask & (OBJECT_MASK_SKY | OBJECT_MASK_TRANSLUCENT | OBJECT_MASK_DEBUG))));
    result.a = 1.0;
    color_output = result;

    // // temp
    // color_output = Texture2D(HYP_SAMPLER_NEAREST, deferred_indirect_lighting, texcoord);

}