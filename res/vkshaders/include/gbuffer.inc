#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#define NUM_GBUFFER_TEXTURES 4
#define HYP_GBUFFER_FLIP_Y 1

#include "shared.inc"

layout(set = 1, binding = 0) uniform sampler2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
layout(set = 1, binding = 1) uniform sampler2D gbuffer_depth_texture;

#define gbuffer_albedo_texture    (gbuffer_textures[0])
#define gbuffer_normals_texture   (gbuffer_textures[1])
#define gbuffer_positions_texture (gbuffer_textures[2])
#define gbuffer_material_texture  (gbuffer_textures[3])

layout(set = 1, binding = 4) uniform sampler2D gbuffer_deferred_result;

#endif