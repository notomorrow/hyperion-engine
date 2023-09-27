#ifndef VCT_UNIFORMS_GLSL
#define VCT_UNIFORMS_GLSL

#include "../defines.inc"

layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 71) uniform VctParams
{
    vec4 extent;
    vec4 aabb_max;
    vec4 aabb_min;
    uvec4 dimensions;
} vct_params;

#endif
