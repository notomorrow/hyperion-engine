
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

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Scene, ScenesBuffer) readonly buffer ScenesBuffer
{
    Scene scene;
};

HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SRV(MergeHalfResTexturesDescriptorSet, InTexture) uniform texture2D src_image;

HYP_DESCRIPTOR_CBUFF(MergeHalfResTexturesDescriptorSet, UniformBuffer) uniform UniformBuffer
{
    uvec2   dimensions;
};

void main()
{
    vec2 texcoord_a = v_texcoord * 0.5;
    vec2 texcoord_b = texcoord_a + vec2(0.5, 0.0);

    uvec2 pixel_coord = uvec2(v_texcoord * (vec2(dimensions) - 1.0));
    const uint pixel_index = pixel_coord.y * dimensions.x + pixel_coord.x;

    vec4 color_a = Texture2D(sampler_nearest, src_image, texcoord_a);
    vec4 color_b = Texture2D(sampler_nearest, src_image, texcoord_b);
    
    color_output = mix(color_a, color_b, float(pixel_index & 1));
}
