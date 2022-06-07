#ifndef HYP_SHADER_DEFINES_GLSL
#define HYP_SHADER_DEFINES_GLSL

#define HYP_MOLTENVK 1 // tmp

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK

#extension GL_EXT_shader_explicit_arithmetic_types : require

#define U32 uint32_t

#else

#define U32 uint // glsl

#endif


#define HYP_FEATURES_BINDLESS_TEXTURES 1

#if HYP_FEATURES_BINDLESS_TEXTURES
    #define HYP_DESCRIPTOR_SET_TEXTURES 6
    #define HYP_DESCRIPTOR_SET_VOXELIZER 8

    //#define HYP_DESCRIPTOR_INDEX_SAMPLER 0
    #define HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY 0

    // none
    #define HYP_MAX_BOUND_TEXTURES
#else
    #define HYP_DESCRIPTOR_SET_TEXTURES 7
    #define HYP_DESCRIPTOR_SET_VOXELIZER 6

    #define HYP_DESCRIPTOR_INDEX_SAMPLER 0
    #define HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY 1

    #define HYP_MAX_BOUND_TEXTURES 16
#endif

#endif
