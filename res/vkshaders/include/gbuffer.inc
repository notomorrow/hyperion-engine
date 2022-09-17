#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define NUM_GBUFFER_TEXTURES 5 // we do 5 not because that is our count in C++ land (minus depth tex), but because translucent and opaque albedo are both stored in the array
#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
// layout : [opaque albedo, .. shared gbuffer textures .., translucent albedo]
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 0) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 1) uniform texture2D gbuffer_depth_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 2) uniform texture2D gbuffer_mip_chain;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 3) uniform sampler gbuffer_depth_sampler; // nearest
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 4) uniform sampler gbuffer_sampler;       // mipmap linear
#endif

#define HYP_SAMPLER_NEAREST gbuffer_depth_sampler
#define HYP_SAMPLER_LINEAR gbuffer_sampler

#define HYP_GBUFFER_TEXTURE(index) (gbuffer_textures[index])

#define gbuffer_albedo_texture HYP_GBUFFER_TEXTURE(0)
#define gbuffer_normals_texture HYP_GBUFFER_TEXTURE(1)
#define gbuffer_material_texture HYP_GBUFFER_TEXTURE(2)
#define gbuffer_tangents_texture HYP_GBUFFER_TEXTURE(3)
#define gbuffer_albedo_texture_translucent HYP_GBUFFER_TEXTURE(NUM_GBUFFER_TEXTURES - 1)


#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 5) uniform texture2D gbuffer_deferred_result;

vec4 SampleGBuffer(texture2D tex, vec2 texcoord)
{
    return Texture2D(gbuffer_depth_sampler, tex, texcoord);
}
#endif

#endif
