#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define NUM_GBUFFER_TEXTURES 4 // do not include depth tex
#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
// we do * 2 so we can have translucent bucket images
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 0) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES * 2];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 1) uniform texture2D gbuffer_depth_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 2) uniform texture2D gbuffer_mip_chain;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 3) uniform sampler gbuffer_depth_sampler; // nearest
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 4) uniform sampler gbuffer_sampler;       // mipmap linear
#endif

#define HYP_SAMPLER_NEAREST gbuffer_depth_sampler
#define HYP_SAMPLER_LINEAR gbuffer_sampler

#define HYP_GBUFFER_TEXTURE_OPAQUE(index) (gbuffer_textures[index])
#define HYP_GBUFFER_TEXTURE_TRANSLUCENT(index) (gbuffer_textures[NUM_GBUFFER_TEXTURES + index])

#define gbuffer_albedo_texture HYP_GBUFFER_TEXTURE_OPAQUE(0)
#define gbuffer_normals_texture HYP_GBUFFER_TEXTURE_OPAQUE(1)
#define gbuffer_material_texture HYP_GBUFFER_TEXTURE_OPAQUE(2)
#define gbuffer_tangents_texture HYP_GBUFFER_TEXTURE_OPAQUE(3)

#define gbuffer_albedo_texture_translucent HYP_GBUFFER_TEXTURE_TRANSLUCENT(0)
#define gbuffer_normals_texture_translucent HYP_GBUFFER_TEXTURE_TRANSLUCENT(1)
#define gbuffer_material_texture_translucent HYP_GBUFFER_TEXTURE_TRANSLUCENT(2)
#define gbuffer_tangents_texture_translucent HYP_GBUFFER_TEXTURE_TRANSLUCENT(3)

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 5) uniform texture2D gbuffer_deferred_result;

vec4 SampleGBuffer(texture2D tex, vec2 texcoord)
{
    return Texture2D(gbuffer_depth_sampler, tex, texcoord);
}
#endif

#endif
