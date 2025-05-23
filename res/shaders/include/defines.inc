#ifndef HYP_SHADER_DEFINES_GLSL
#define HYP_SHADER_DEFINES_GLSL

#define HYP_MOLTENVK 0 // tmp

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK

#extension GL_EXT_shader_explicit_arithmetic_types : require

#define U32 uint32_t

#else

#define U32 uint // glsl

#endif

#define UINT_TO_VEC4(_value) \
    (unpackUnorm4x8((_value)))

#define UINT_TO_VEC2(_value) \
    (unpackUnorm2x16((_value)))

#define VEC4_TO_UINT(_value) \
    (packUnorm4x8((_value)))

#define VEC2_TO_UINT(_value) \
    (packUnorm2x16((_value)))

#define HYP_MATERIAL_CUBEMAP_TEXTURES  1

#define HYP_DESCRIPTOR_SET_GLOBAL    1
#define HYP_DESCRIPTOR_SET_SCENE     2
#define HYP_DESCRIPTOR_SET_VOXELIZER 3
#define HYP_DESCRIPTOR_SET_OBJECT    4
#define HYP_DESCRIPTOR_SET_TEXTURES  5

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
    #define HYP_DESCRIPTOR_SET_RAYTRACING 6

    #define HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY 0

    // none
    #define HYP_MAX_BOUND_TEXTURES
#else
    #define HYP_DESCRIPTOR_SET_RAYTRACING 9

    #define HYP_DESCRIPTOR_INDEX_TEXTURES_ARRAY 1

    #define HYP_MAX_BOUND_TEXTURES 16
#endif

#define HYP_MAX_BOUND_AMBIENT_PROBES 4096
#define HYP_MAX_BOUND_REFLECTION_PROBES 16
#define HYP_MAX_BOUND_POINT_SHADOW_MAPS 16
#define HYP_MAX_SHADOW_MAPS 4
#define HYP_MAX_BOUND_ENVIRONMENT_MAPS 1
#define HYP_MAX_MATERIALS 65536
#define HYP_MAX_ENTITIES 131072

#define HYP_LIGHT_TYPE_DIRECTIONAL 0
#define HYP_LIGHT_TYPE_POINT       1
#define HYP_LIGHT_TYPE_SPOT        2
#define HYP_LIGHT_TYPE_AREA_RECT   3

#define HYP_OBJECT_BUCKET_NONE          0
#define HYP_OBJECT_BUCKET_OPAQUE        1
#define HYP_OBJECT_BUCKET_LIGHTMAP      2
#define HYP_OBJECT_BUCKET_TRANSLUCENT   3
#define HYP_OBJECT_BUCKET_SKYBOX        4
#define HYP_OBJECT_BUCKET_DEBUG         5

#define HYP_FMATH_SQR(num)     ((num)*(num))
#define HYP_FMATH_CUBE(num)    ((num)*(num)*(num))
#define HYP_FMATH_PI           3.14159265359
#define HYP_FMATH_PI_HALF      1.57079632679
#define HYP_FMATH_TWO_PI       6.28318530718
#define HYP_FMATH_ONE_OVER_PI  0.31830988618
#define HYP_FMATH_QUARTER_PI   0.7853981625
#define HYP_FMATH_DEG2RAD(deg) ((deg) * HYP_FMATH_PI / 180.0)
#define HYP_FMATH_RAD2DEG(rad) ((rad) * 180.0 / HYP_FMATH_PI)
#define HYP_FMATH_EPSILON      0.00000001
#define HYP_FMATH_INFINITY     99999999.0

#define HYP_FLOAT_MAX           3.402823466e+38F
#define HYP_FLOAT_MIN           1.175494351e-38F

#ifdef __COUNTER__
    #define HYP_UNIQUE_NAME(prefix) \
        HYP_CONCAT(prefix, __COUNTER__)
#else
    #define HYP_UNIQUE_NAME(prefix) \
        prefix
#endif

#define HYP_PAD_STRUCT_HERE(type, count) \
    type HYP_UNIQUE_NAME(_padding)[count]

#endif
