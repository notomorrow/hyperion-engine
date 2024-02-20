
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

#include "include/gbuffer.inc"
#include "include/shared.inc"
#include "include/tonemap.inc"
#include "include/PostFXSample.inc"
#include "include/rt/probe/probe_uniforms.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba8) uniform image2D image_storage_test;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 39) uniform texture2D ssr_result;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 36) uniform texture2D depth_pyramid_result;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 42) uniform texture2D ui_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 41) uniform texture2D hbao_gi;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 43) uniform texture2D motion_vectors_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 50) uniform texture2D temporal_aa_result;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 59) uniform texture2D reflection_probes_texture;
// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 79) uniform texture2D dof_blur_blended;

layout(location=0) out vec4 out_color;

void main()
{
    out_color = vec4(0.0, 0.0, 0.0, 1.0);
#if 1
#ifdef TEMPORAL_AA
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, temporal_aa_result, v_texcoord0).rgb;
#else
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, gbuffer_deferred_result, v_texcoord0).rgb;
#endif
#else
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, dof_blur_blended, v_texcoord0).rgb;
#endif

    out_color = SampleLastEffectInChain(HYP_STAGE_POST, v_texcoord0, out_color);

    const bool is_sky = bool(VEC4_TO_UINT(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_mask_texture, v_texcoord0)) & 0x10);
    //out_color = vec4(mix(out_color.rgb, Tonemap(out_color.rgb), bvec3(!is_sky)), 1.0);


#if defined(DEBUG_SSR)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, ssr_result, v_texcoord0).rgb;
#elif defined(DEBUG_HBAO)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, hbao_gi, v_texcoord0).aaa;
#elif defined(DEBUG_HBIL)
    out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, hbao_gi, v_texcoord0).rgb;

    // out_color.rgb = (Texture2D(HYP_SAMPLER_NEAREST, hbao_gi, v_texcoord0).rgb - vec3(Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, v_texcoord0).rrr)) * 6.0;
#endif
    out_color.a = 1.0;

    out_color.rgb = Tonemap(out_color.rgb);

    // blend in UI.
    vec4 ui_color = Texture2D(HYP_SAMPLER_LINEAR, ui_texture, v_texcoord0);

    out_color = vec4(
        (ui_color.rgb * ui_color.a) + (out_color.rgb * (1.0 - ui_color.a)),
        1.0
    );

    out_color = any(isnan(out_color)) ? vec4(0.0, 1.0, 0.0, 65535.0) : out_color;
    


    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, reflection_probes_texture, v_texcoord0).rgb;

    // out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_depth_texture, v_texcoord0).rrr;
    // out_color.rgb = Texture2D(HYP_SAMPLER_NEAREST, ssr_result, v_texcoord0).rgb;

    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, light_field_filtered_distance_buffer, v_texcoord0 * 2.0).rrr;
    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, light_field_irradiance_buffer, v_texcoord0).rgb;

    // out_color.rgb = Texture2D(HYP_SAMPLER_LINEAR, depth_pyramid_result, v_texcoord0).rgb;
}