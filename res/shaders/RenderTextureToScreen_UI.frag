
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord;

layout(location=0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/shared.inc"

#include "include/scene.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer, size = 512) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer, size = 256) readonly buffer ScenesBuffer
{
    Scene scene;
};

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_SRV(RenderTextureToScreenDescriptorSet, InTexture) uniform texture2D src_image;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

void main()
{
#if 0
    uvec2 screen_resolution = uvec2(camera.dimensions.xy);
    uvec2 pixel_coord = uvec2(v_texcoord * (vec2(screen_resolution) - 1.0));
    const uint pixel_index = pixel_coord.y * screen_resolution.x + pixel_coord.x;

    // @NOTE: == is flipped relative to shaders that use checkerboard rendering
    if ((pixel_coord.x & (pixel_coord.y & 1)) == (scene.frame_counter & 1))
    {
        color_output = vec4(0.0);
        return;
    }
#endif

    color_output = Texture2D(sampler_linear, src_image, v_texcoord);
}
