
#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texcoord;

layout(location = 0) out vec4 color_output;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/shared.inc"

#include "include/scene.inc"

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Global, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Global, WorldsBuffer) readonly buffer WorldsBuffer
{
    WorldShaderData world_shader_data;
};

HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler sampler_nearest;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler sampler_linear;

HYP_DESCRIPTOR_SRV(MergeHalfResTexturesDescriptorSet, InTexture) uniform texture2D src_image;

HYP_DESCRIPTOR_CBUFF(MergeHalfResTexturesDescriptorSet, UniformBuffer) uniform UniformBuffer
{
    uvec2 dimensions;
};

void main()
{
    vec2 texcoord_a = v_texcoord * vec2(0.5, 1.0);
    vec2 texcoord_b = texcoord_a + vec2(0.5, 0.0);

    uvec2 pixel_coord = uvec2(v_texcoord * (vec2(dimensions) - 1.0));
    uint checkerboard = (pixel_coord.x & 1) ^ (pixel_coord.y & 1);

    vec2 texcoord = mix(texcoord_a, texcoord_b, float(checkerboard));

    color_output = Texture2D(sampler_nearest, src_image, texcoord);
}
