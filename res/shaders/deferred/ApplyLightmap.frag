#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../include/defines.inc"

layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(Scene, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
#else
HYP_DESCRIPTOR_SRV(Scene, GBufferAlbedoTexture) uniform texture2D gbuffer_albedo_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferNormalsTexture) uniform texture2D gbuffer_normals_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferMaterialTexture) uniform texture2D gbuffer_material_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferVelocityTexture) uniform texture2D gbuffer_velocity_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferLightmapTexture) uniform texture2D gbuffer_albedo_lightmap_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferMaskTexture) uniform texture2D gbuffer_mask_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferWSNormalsTexture) uniform texture2D gbuffer_ws_normals_texture;
HYP_DESCRIPTOR_SRV(Scene, GBufferTranslucentTexture) uniform texture2D gbuffer_albedo_texture_translucent;
#endif

HYP_DESCRIPTOR_SRV(Scene, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Scene, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest)
uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear)
uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_final;

HYP_DESCRIPTOR_SRV(Scene, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(Scene, TAAResultTexture) uniform texture2D temporal_aa_result;
HYP_DESCRIPTOR_SRV(Scene, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(Scene, SSAOResultTexture) uniform texture2D ssao_gi;
HYP_DESCRIPTOR_SRV(Scene, DeferredIndirectResultTexture) uniform texture2D deferred_indirect_lighting;

HYP_DESCRIPTOR_CBUFF(Scene, PostProcessingUniforms) uniform PostProcessingUniforms
{
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
    uvec2 _pad;
}
post_processing;

#include "../include/shared.inc"
#include "../include/gbuffer.inc"
#include "../include/object.inc"
#include "../include/PostFXSample.inc"
#include "../include/tonemap.inc"

#include "../include/scene.inc"

#ifdef HYP_FEATURES_DYNAMIC_DESCRIPTOR_INDEXING
HYP_DESCRIPTOR_SRV(Scene, PostFXPreStack, count = 4) uniform texture2D effects_pre_stack[4];
HYP_DESCRIPTOR_SRV(Scene, PostFXPostStack, count = 4) uniform texture2D effects_post_stack[4];
#endif

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, WorldsBuffer) uniform WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SSBO(Global, ShadowMapsBuffer) readonly buffer ShadowMapsBuffer
{
    ShadowMap shadow_map_data[];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentLight) readonly buffer CurrentLight
{
    Light light;
};

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray) uniform texture2DArray shadow_maps;
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray) uniform textureCubeArray point_shadow_maps;

#include "../include/brdf.inc"

#include "../include/env_probe.inc"
HYP_DESCRIPTOR_SRV(Global, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];
HYP_DESCRIPTOR_SSBO(Global, EnvProbesBuffer) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[];
};
HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, EnvGridsBuffer) uniform EnvGridsBuffer
{
    EnvGrid env_grid;
};
HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, CurrentEnvProbe) readonly buffer CurrentEnvProbe
{
    EnvProbe current_env_probe;
};
HYP_DESCRIPTOR_SRV(Scene, ReflectionProbeResultTexture) uniform texture2D reflections_texture;

HYP_DESCRIPTOR_SRV(Global, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Global, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

#include "./DeferredLighting.glsl"
#include "../include/shadows.inc"

#define HYP_DEFERRED_NO_RT_RADIANCE // temp
// #define HYP_DEFERRED_NO_ENV_PROBE // temp

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

void main()
{
    vec4 result = vec4(0.0);

    vec4 albedo = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture, texcoord);
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

    // apply reflections to lightmapped objects
    if (bool(object_mask & OBJECT_MASK_LIGHTMAP))
    {
        vec3 ibl = vec3(0.0);
        vec3 F = vec3(0.0);

        float NdotV = max(0.0001, dot(N, V));

        const vec3 diffuse_color = CalculateDiffuseColor(albedo.rgb, metalness);
        const vec3 F0 = CalculateF0(albedo.rgb, metalness);

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

        vec4 lightmap = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_lightmap_texture, texcoord);

        result = (albedo * lightmap) + vec4(spec, 0.0);
    }

    color_output = result;
}