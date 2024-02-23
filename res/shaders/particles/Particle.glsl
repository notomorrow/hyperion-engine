#ifndef HYP_PARTICLE_GLSL
#define HYP_PARTICLE_GLSL

struct ParticleShaderData
{
    vec4 position; // alpha is scale
    vec4 velocity; // alpha is final scale
    vec4 color;
    vec4 attributes; // x = lifetime
};

#endif