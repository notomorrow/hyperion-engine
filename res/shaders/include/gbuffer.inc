#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define NUM_GBUFFER_TEXTURES 8
#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
// layout : [opaque albedo, .. shared gbuffer textures .., translucent albedo]
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 0) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 1) uniform texture2D gbuffer_depth_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 2) uniform texture2D gbuffer_mip_chain;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 3) uniform sampler sampler_nearest; // nearest
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 4) uniform sampler sampler_linear;       // mipmap linear
#endif

#define gbuffer_sampler sampler_linear
#define gbuffer_depth_sampler sampler_nearest
#define HYP_SAMPLER_NEAREST gbuffer_depth_sampler
#define HYP_SAMPLER_LINEAR gbuffer_sampler

#define HYP_GBUFFER_TEXTURE(index) (gbuffer_textures[index])

#define gbuffer_albedo_texture HYP_GBUFFER_TEXTURE(0)
#define gbuffer_normals_texture HYP_GBUFFER_TEXTURE(1)
#define gbuffer_material_texture HYP_GBUFFER_TEXTURE(2)
#define gbuffer_tangents_texture HYP_GBUFFER_TEXTURE(3)
#define gbuffer_velocity_texture HYP_GBUFFER_TEXTURE(4)
#define gbuffer_mask_texture HYP_GBUFFER_TEXTURE(5)
#define gbuffer_ws_normals_texture HYP_GBUFFER_TEXTURE(6)
#define gbuffer_albedo_texture_translucent HYP_GBUFFER_TEXTURE(NUM_GBUFFER_TEXTURES - 1)

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 5) uniform texture2D gbuffer_deferred_result;

vec4 SampleGBuffer(texture2D tex, vec2 texcoord)
{
    return Texture2D(gbuffer_depth_sampler, tex, texcoord);
}

float SampleDepth(vec2 uv)
{
    return SampleGBuffer(gbuffer_depth_texture, uv).r;
}
#endif

#endif
