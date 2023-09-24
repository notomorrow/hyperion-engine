#ifndef HYP_GAUSSIAN_GLSL
#define HYP_GAUSSIAN_GLSL

struct GaussianSplatShaderData
{
    vec4 position;
    vec4 rotation; // quaternion
    vec4 covariance0;
    vec4 covariance1;
    vec4 color;
};

layout(std430, set = 0, binding = 0, row_major) buffer GaussianBuffer
{
    GaussianSplatShaderData instances[];
};

#endif