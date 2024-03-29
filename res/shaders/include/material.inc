#ifndef HYP_MATERIAL_GLSL
#define HYP_MATERIAL_GLSL

#include "defines.inc"
#include "shared.inc"

struct Material
{
    vec4 albedo;
    
    uvec4 packed_params;

    vec2 uv_scale;
    float parallax_height;
    float _pad0;
    
    uvec4 texture_indices[4]; // moltenvk is compiling uint32_t texture_index[16] -> uint4 texture_index[16]
    
    uint texture_usage;
    uint _pad1;
    uint _pad2;
    uint _pad3;
};

// enum for packed params
#define MATERIAL_PARAM_ROUGHNESS 0
#define MATERIAL_PARAM_METALNESS 1
#define MATERIAL_PARAM_TRANSMISSION 2
#define MATERIAL_PARAM_NORMAL_MAP_INTENSITY 3
#define MATERIAL_PARAM_ALPHA_THRESHOLD 4
#define MATERIAL_PARAM_UI_BACKGROUND_COLOR 15

#define GET_MATERIAL_PARAM(mat, index) \
    unpackUnorm4x8(mat.packed_params[(index) / 4])[(index) % 4]

#define MATERIAL_TEXTURE_ALBEDO_map     0
#define MATERIAL_TEXTURE_NORMAL_MAP     1
#define MATERIAL_TEXTURE_AO_MAP         2
#define MATERIAL_TEXTURE_PARALLAX_MAP   3
#define MATERIAL_TEXTURE_METALNESS_MAP  4
#define MATERIAL_TEXTURE_ROUGHNESS_MAP  5
#define MATERIAL_TEXTURE_RADIANCE_MAP   6
#define MATERIAL_TEXTURE_IRRADIANCE_MAP 7

#define MATERIAL_ALPHA_DISCARD         0.25

#define HAS_TEXTURE(mat, index) \
    (((mat).texture_usage & (1 << index)) != 0)

#define GET_TEXTURE(mat, index) \
    textures[(mat).texture_indices[(index / 4)][index % 4]]

#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
    #define GET_TEXTURE_CUBE(mat, index) \
        cubemap_textures[(mat).texture_indices[(index / 4)][index % 4]]
#endif

#define SAMPLE_TEXTURE(mat, index, texcoord) \
    Texture2D(texture_sampler, GET_TEXTURE(mat, index), (texcoord))

#define SAMPLE_TEXTURE_LOD(mat, index, texcoord, lod) \
    Texture2DLod(texture_sampler, GET_TEXTURE(mat, index), (texcoord), (lod))

#define SAMPLE_TEXTURE_NEAREST(mat, index, texcoord) \
    Texture2D(sampler_nearest, GET_TEXTURE(mat, index), (texcoord))

#define SAMPLE_TEXTURE_TRIPLANAR(mat, index, position, normal) \
    Texture2DTriplanar(texture_sampler, GET_TEXTURE(mat, index), (position), (normal))

#define SAMPLE_TEXTURE_CUBE(mat, index, texcoord) \
    TextureCube(texture_sampler, GET_TEXTURE_CUBE(mat, index), (texcoord))

#endif