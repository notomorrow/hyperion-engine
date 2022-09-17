#version 450
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout     : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

#include "include/gbuffer.inc"
#include "include/shared.inc"
#include "include/tonemap.inc"
#include "include/PostFXSample.inc"
#include "include/rt/probe/probe_uniforms.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba8) uniform image2D image_storage_test;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 17) uniform texture2D ssr_uvs;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 19) uniform texture2D ssr_radius;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 20) uniform texture2D ssr_blur_hor;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 36) uniform texture2D depth_pyramid_result;

layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 1, rgba16f)  uniform image2D rt_image;
layout(set = HYP_DESCRIPTOR_SET_RAYTRACING, binding = 11, rgba16f) uniform image2D irradiance_image;
//layout(set = 9, binding = 12, rg16f)   uniform image2D depth_image;

layout(location=0) out vec4 out_color;

void main()
{
    vec4 albedo = vec4(0.0);


    //out_color = texture(shadow_map, texcoord);

    // if (post_processing.masks[HYP_STAGE_POST] != 0) {
    //     out_color = Texture2D(HYP_SAMPLER_NEAREST, effects_post_stack[post_processing.last_enabled_indices[HYP_STAGE_POST]], v_texcoord0);
    // } else {
        // out_color = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0);
    // }

    //out_color = vec4(Tonemap(out_color.rgb), 1.0);

    
    //out_color = imageLoad(rt_image, ivec2(int(v_texcoord0.x * float(imageSize(rt_image).x)), int(v_texcoord0.y * float(imageSize(rt_image).y))));

   // ivec2 size = imageSize(irradiance_image);
   // out_color = imageLoad(irradiance_image, ivec2(int(v_texcoord0.x * float(size.x)), int(v_texcoord0.y * float(size.y))));
    vec4 deferred_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_deferred_result, v_texcoord0);
    out_color = deferred_result;

#if 0
    vec4 material = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_material_texture_translucent, v_texcoord0);
    const float roughness = material.r;
    const float metalness = material.g;
    const float transmission = material.b;

    vec4 translucent_result = Texture2D(HYP_SAMPLER_NEAREST, gbuffer_albedo_texture_translucent, v_texcoord0);
    if (transmission > 0.0) {

        const float IOR = 1.5;
        const float material_reflectance = 0.5;
        // dialetric f0
        const float reflectance = 0.16 * material_reflectance * material_reflectance;
        const vec3 F0 = translucent_result.rgb * metalness + (reflectance * (1.0 - metalness));

        const float sqrt_F0 = sqrt(F0.y);
        const float material_ior = (1.0 + sqrt_F0) / (1.0 - sqrt_F0);
        const float air_ior = 1.0;
        const float eta_ir = air_ior / material_ior;
        const float eta_ri = material_ior / air_ior;

        float perceptual_roughness = sqrt(roughness);
        perceptual_roughness = mix(perceptual_roughness, 0.0, Saturate(eta_ir * 3.0 - 2.0));

        const float inv_log2_sqrt5 = 0.8614;
        const float lod = max(0.0, (2.0 * log2(perceptual_roughness)) * inv_log2_sqrt5);

        vec3 Ft = vec3(1.0);//deferred_result.rgb * (lod);//Texture2DLod(HYP_SAMPLER_LINEAR, gbuffer_mip_chain, v_texcoord0, lod).rgb;

        Ft *= translucent_result.rgb;
        // Ft *= 1.0 - E
        // Ft *= absorption;
        Ft *= transmission;

        // out_color = vec4(Ft + (translucent_result.rgb * (1.0 - transmission)), 1.0);
        out_color = vec4(translucent_result.rgb, 1.0);
    } else {
        out_color = deferred_result;
    }
#endif

    out_color = SampleLastEffectInChain(HYP_STAGE_POST, v_texcoord0, out_color);
    out_color = vec4(Tonemap(out_color.rgb), 1.0);

    // out_color = SampleGBuffer(gbuffer_material_texture, v_texcoord0);
}
