#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

HYP_DESCRIPTOR_SAMPLER(Global, SamplerLinear) uniform sampler2D sampler_linear;
HYP_DESCRIPTOR_SAMPLER(Global, SamplerNearest) uniform sampler2D sampler_nearest;

HYP_DESCRIPTOR_SRV(DilateLightmapDescriptorSet, InImage) uniform texture2D input_texture;
HYP_DESCRIPTOR_UAV(DilateLightmapDescriptorSet, OutImage, format = rgba8) uniform writeonly image2D output_image;

HYP_DESCRIPTOR_CBUFF(DilateLightmapDescriptorSet, DilateLightmapUniforms) uniform DilateLightmapUniforms
{
    uvec2 input_texture_size;
};

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "./include/defines.inc"
#include "./include/packing.inc"
#include "./include/defines.inc"
#include "./include/packing.inc"
#include "./include/shared.inc"

void main(void)
{
    const uvec2 input_coord = uvec2(gl_GlobalInvocationID.xy);

    if (input_coord.x >= input_texture_size.x || input_coord.y >= input_texture_size.y)
    {
        return;
    }

    const vec2 texel_size = 1.0 / vec2(input_texture_size);

    vec4 c = Texture2D(sampler_nearest, input_texture, (vec2(input_coord)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) - texel_size) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(0.0, -texel_size)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(texel_size, -texel_size)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(-texel_size, 0.0)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(texel_size, 0.0)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(-texel_size, texel_size)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + vec2(0.0, texel_size)) * texel_size);
    c = c.a > 0.0 ? c : Texture2D(sampler_nearest, input_texture, (vec2(input_coord) + texel_size) * texel_size);

    imageStore(output_image, ivec2(input_coord), c);
}
