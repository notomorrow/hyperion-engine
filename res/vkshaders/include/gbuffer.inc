
#define NUM_GBUFFER_TEXTURES 4
#define HYP_GBUFFER_FLIP_Y 1

layout(set = 1, binding = 0) uniform sampler2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
layout(set = 1, binding = 1) uniform sampler2D gbuffer_depth_texture;

#define gbuffer_albedo_texture    (gbuffer_textures[0])
#define gbuffer_normals_texture   (gbuffer_textures[1])
#define gbuffer_positions_texture (gbuffer_textures[2])
#define gbuffer_material_texture  (gbuffer_textures[3])

layout(set = 1, binding = 4) uniform sampler2D gbuffer_deferred_result;

vec4 Texture2D(sampler2D tex, vec2 texcoord)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(tex, vec2(texcoord.s, 1.0 - texcoord.t));
#else
    return texture(tex, texcoord);
#endif
}

vec4 Texture2DLod(sampler2D tex, vec2 texcoord, float lod)
{
#if HYP_GBUFFER_FLIP_Y
    return texture(tex, vec2(texcoord.s, 1.0 - texcoord.t), lod);
#else
    return texture(tex, texcoord, lod);
#endif
}