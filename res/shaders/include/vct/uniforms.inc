#ifndef VCT_UNIFORMS_GLSL
#define VCT_UNIFORMS_GLSL

#include "../defines.inc"

struct VCTParams
{
    vec4 extent;
    vec4 aabb_max;
    vec4 aabb_min;
    uvec4 dimensions;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 71) uniform VctParams
{
    VCTParams vct_params;
};
#endif

#endif
