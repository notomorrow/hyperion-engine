
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

HYP_DESCRIPTOR_SRV(Global, PostFXPreStack, count = 4) uniform texture2D effects_pre_stack[4];
HYP_DESCRIPTOR_SRV(Global, PostFXPostStack, count = 4) uniform texture2D effects_post_stack[4];

HYP_DESCRIPTOR_CBUFF(Global, PostProcessingUniforms, size = 32) uniform PostProcessingUniforms {
    uvec2 effect_counts;
    uvec2 last_enabled_indices;
    uvec2 masks;
    uvec2 _pad;
} post_processing;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/gbuffer.inc"
#include "include/shared.inc"
#include "include/tonemap.inc"
#include "include/PostFXSample.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(Global, GBufferTextures, count = 8) uniform texture2D gbuffer_textures[8];
HYP_DESCRIPTOR_SRV(Global, GBufferMipChain) uniform texture2D gbuffer_mip_chain;
HYP_DESCRIPTOR_SRV(Global, GBufferDepthTexture) uniform texture2D gbuffer_depth_texture;
HYP_DESCRIPTOR_SRV(Global, DepthPyramidResult) uniform texture2D depth_pyramid;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler depth_pyramid_sampler;
HYP_DESCRIPTOR_SRV(Global, DeferredIndirectResultTexture) uniform texture2D deferred_indirect_texture;
HYP_DESCRIPTOR_SRV(Global, DeferredDirectResultTexture) uniform texture2D deferred_direct_texture;
HYP_DESCRIPTOR_SRV(Global, DeferredResult) uniform texture2D gbuffer_deferred_result;
HYP_DESCRIPTOR_SRV(Global, SSRResultTexture) uniform texture2D ssr_result;
HYP_DESCRIPTOR_SRV(Global, SSGIResultTexture) uniform texture2D ssgi_result;
HYP_DESCRIPTOR_SRV(Global, SSAOResultTexture) uniform texture2D ssao_gi;
HYP_DESCRIPTOR_SRV(Global, TAAResultTexture) uniform texture2D temporal_aa_result;
HYP_DESCRIPTOR_SRV(Global, RTRadianceResultTexture) uniform texture2D rt_radiance_result;
HYP_DESCRIPTOR_SRV(Scene, EnvProbeTextures, count = 16) uniform texture2D env_probe_textures[16];

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;
// HYP_DESCRIPTOR_SRV(Scene, EnvGridProbeDataTexture) uniform texture2D env_grid_probe_data;
HYP_DESCRIPTOR_SRV(Scene, ShadowMapTextures, count = 16) uniform texture2D shadow_maps[16];

layout(location=0) out vec4 out_color;

void main()
{
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
#if 1
#ifdef TEMPORAL_AA
    out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, temporal_aa_result, v_texcoord0).rgb;
#else
    out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0).rgb;
#endif
#else
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, dof_blur_blended, v_texcoord0).rgb;
#endif

    out_color = SampleLastEffectInChain(HYP_STAGE_POST, v_texcoord0, out_color);

    out_color = vec4(Tonemap(out_color.rgb), 1.0);

#if defined(DEBUG_SSR)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, ssr_result, v_texcoord0).rgb;
#elif defined(DEBUG_SSGI)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, ssgi_result, v_texcoord0).rgb;
#elif defined(DEBUG_HBAO)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi, v_texcoord0).aaa;
#elif defined(DEBUG_HBIL)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, ssao_gi, v_texcoord0).rgb;

    // out_color.rgb = (Texture2D(HYP_SAMPLER_NEAREST, hbao_gi, v_texcoord0).rgb - vec3(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, v_texcoord0).rrr)) * 6.0;
#endif
    out_color.a = 1.0;


    // out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, depth_pyramid, v_texcoord0).rgb;
    
    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, env_probe_textures[1], v_texcoord0).rgb;
    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, light_field_depth_texture, v_texcoord0).rgb;

    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, gbuffer_albedo_texture, v_texcoord0).rgb;

    // out_color.rgb = vec3(float(is_sky));
    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, deferred_indirect_texture, v_texcoord0).rgb;
    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, gbuffer_deferred_result, v_texcoord0).rgb;

    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, shadow_maps[0], v_texcoord0).rgb;
}