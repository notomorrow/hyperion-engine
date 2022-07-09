#version 450
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout     : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

#include "include/gbuffer.inc"
#include "include/shared.inc"
#include "include/tonemap.inc"
#include "include/post_fx.inc"
#include "include/rt/probe/probe_uniforms.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 16, rgba8) uniform image2D image_storage_test;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 17) uniform texture2D ssr_uvs;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 18) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 19) uniform texture2D ssr_radius;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 20) uniform texture2D ssr_blur_hor;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;

//layout(set = 9, binding = 1, rgba16f)  uniform image2D rt_image;
//layout(set = 9, binding = 11, rgba16f) uniform image2D irradiance_image;
//layout(set = 9, binding = 12, rg16f)   uniform image2D depth_image;

layout(location=0) out vec4 out_color;

void main()
{
    vec4 albedo = vec4(0.0);

    //ivec2 size = imageSize(irradiance_image);
    //out_color = imageLoad(irradiance_image, ivec2(int(v_texcoord0.x * float(size.x)), int(v_texcoord0.y * float(size.y))));
    

    //out_color = imageLoad(rt_image, ivec2(int(v_texcoord0.x * float(imageSize(rt_image).x)), int(v_texcoord0.y * float(imageSize(rt_image).y))));

    //out_color = texture(shadow_map, texcoord);

    if (post_processing.masks[HYP_STAGE_POST] != 0) {
        out_color = Texture2D(effects_post_stack[post_processing.last_enabled_indices[HYP_STAGE_POST]], v_texcoord0);
    } else {
        out_color = Texture2D(gbuffer_deferred_result, v_texcoord0);
    }

    out_color = vec4(Tonemap(out_color.rgb), 1.0);

    //out_color = Texture2D(gbuffer_sampler, ssr_radius, v_texcoord0);
}
