#ifndef HYP_PARTICLE_GLSL
#define HYP_PARTICLE_GLSL

struct ParticleShaderData
{
    vec4 position; // alpha is scale
    vec4 velocity; // alpha is final scale
    vec4 color;
    vec4 attributes; // x = lifetime
};

layout(std430, set = 0, binding = 0, row_major) buffer ParticleBuffer
{
    ParticleShaderData instances[];
};

#endif