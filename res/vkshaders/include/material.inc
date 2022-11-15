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
#define MATERIAL_PARAM_UI_BACKGROUND_COLOR 15

#define GET_MATERIAL_PARAM(index) \
    unpackUnorm4x8(material.packed_params[(index) / 4])[(index) % 4]

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
    layout(std140, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 0) readonly buffer MaterialBuffer
    {
        Material material;
    };

    #ifdef HYP_DESCRIPTOR_INDEX_SAMPLER
        layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_SAMPLER) uniform sampler texture_sampler;
        layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform texture2D textures[HYP_MAX_BOUND_TEXTURES];
        #if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
            layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform textureCube cubemap_textures[HYP_MAX_BOUND_TEXTURES];
        #endif
    #else
        layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform sampler2D textures[HYP_MAX_BOUND_TEXTURES];
        #if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
            layout(set = HYP_DESCRIPTOR_SET_TEXTURES, binding = HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY) uniform samplerCube cubemap_textures[HYP_MAX_BOUND_TEXTURES];
        #endif
    #endif
#endif

#define MATERIAL_TEXTURE_ALBEDO_map    0
#define MATERIAL_TEXTURE_NORMAL_MAP    1
#define MATERIAL_TEXTURE_AO_MAP        2
#define MATERIAL_TEXTURE_PARALLAX_MAP  3
#define MATERIAL_TEXTURE_METALNESS_MAP 4
#define MATERIAL_TEXTURE_ROUGHNESS_MAP 5
#define MATERIAL_TEXTURE_SKYBOX_MAP    6
#define MATERIAL_TEXTURE_COLOR_MAP     7
#define MATERIAL_TEXTURE_POSITION_MAP  8
#define MATERIAL_TEXTURE_DATA_MAP      9
#define MATERIAL_TEXTURE_SSAO_MAP      10
#define MATERIAL_TEXTURE_TANGENT_MAP   11
#define MATERIAL_TEXTURE_BITANGENT_MAP 12
#define MATERIAL_TEXTURE_DEPTH_MAP     13

#define MATERIAL_ALPHA_DISCARD         0.2

#define HAS_TEXTURE(index) \
    ((material.texture_usage & (1 << index)) != 0)

#define GET_TEXTURE(index) \
    textures[material.texture_indices[(index / 4)][index % 4]]

#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
    #define GET_TEXTURE_CUBE(index) \
        cubemap_textures[material.texture_indices[(index / 4)][index % 4]]
#endif

#ifdef HYP_DESCRIPTOR_INDEX_SAMPLER
    #define SAMPLE_TEXTURE(index, texcoord) \
        Texture2D(texture_sampler, GET_TEXTURE(index), (texcoord))

    #define SAMPLE_TEXTURE_TRIPLANAR(index, position, normal) \
        Texture2DTriplanar(texture_sampler, GET_TEXTURE(index), (position), (normal))

    #define SAMPLE_TEXTURE_CUBE(index, texcoord) \
        TextureCube(texture_sampler, GET_TEXTURE_CUBE(index), (texcoord))
#else
    #define SAMPLE_TEXTURE(index, texcoord) \
        Texture2D(GET_TEXTURE(index), (texcoord))

    #define SAMPLE_TEXTURE_TRIPLANAR(index, position, normal) \
        Texture2DTriplanar(GET_TEXTURE(index), (position), (normal))

    #if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
        #define SAMPLE_TEXTURE_CUBE(index, texcoord) \
            TextureCube(GET_TEXTURE_CUBE(index), (texcoord))
    #endif
#endif

#endif