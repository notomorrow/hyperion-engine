#ifndef HYP_PARTICLE_GLSL
#define HYP_PARTICLE_GLSL

struct ParticleShaderData
{
    vec4 position;
    vec4 direction;
    vec4 velocity;
    float lifetime;
    uint color_packed;
};

layout(std140, set = 0, binding = 0, row_major) buffer ParticleBuffer
{
    ParticleShaderData instances[];
};

#endif