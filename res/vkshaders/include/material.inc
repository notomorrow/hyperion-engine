#ifndef HYP_MATERIAL_GLSL
#define HYP_MATERIAL_GLSL

#include "defines.inc"
#include "shared.inc"

struct Material {
    vec4 albedo;
    
    float metalness;
    float roughness;
    float param_0;
    float param_1;

    U32 uv_flip_s;
    U32 uv_flip_t;
    float uv_scale;
    float parallax_height;
    
    uvec4 texture_indices[4]; // moltenvk is compiling uint32_t texture_index[16] -> uint4 texture_index[16]
    
    U32 texture_usage;
    U32 _pad0;
    U32 _pad1;
    U32 _pad2;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
    layout(std140, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 0) readonly buffer MaterialBuffer {
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