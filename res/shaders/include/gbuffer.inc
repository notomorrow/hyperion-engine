#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define NUM_GBUFFER_TEXTURES 8
#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

#define gbuffer_sampler sampler_linear
#define gbuffer_depth_sampler sampler_nearest

#ifndef sampler_linear
#define HYP_SAMPLER_NEAREST sampler_nearest
#endif

#ifndef HYP_SAMPLER_LINEAR
#define HYP_SAMPLER_LINEAR sampler_linear
#endif

#define HYP_GBUFFER_TEXTURE(index) (gbuffer_textures[index])

#define gbuffer_albedo_texture HYP_GBUFFER_TEXTURE(0)
#define gbuffer_normals_texture HYP_GBUFFER_TEXTURE(1)
#define gbuffer_material_texture HYP_GBUFFER_TEXTURE(2)
#define gbuffer_albedo_lightmap_texture HYP_GBUFFER_TEXTURE(3)
#define gbuffer_velocity_texture HYP_GBUFFER_TEXTURE(4)
#define gbuffer_mask_texture HYP_GBUFFER_TEXTURE(5)
#define gbuffer_ws_normals_texture HYP_GBUFFER_TEXTURE(6)
#define gbuffer_albedo_texture_translucent HYP_GBUFFER_TEXTURE(NUM_GBUFFER_TEXTURES - 1)

#endif
