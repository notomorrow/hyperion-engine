#ifndef HYP_GAUSSIAN_GLSL
#define HYP_GAUSSIAN_GLSL

struct GaussianSplatIndex {
    uint index;
    float distance;
};

struct GaussianSplatShaderData
{
    vec4 position;
    vec4 rotation; // quaternion
    vec4 scale;
    vec4 color;
};

#endif