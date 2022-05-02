#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

#define M_PI 3.14159265359

#include "probe_uniforms.inc"

uvec3 ProbeIndexToGridPosition(uint index)
{
    const uint probe_count_xy = probe_system.probe_counts.x * probe_system.probe_counts.y;
    
    return uvec3(
        index % probe_system.probe_counts.x,
        (index % probe_count_xy) / probe_system.probe_counts.x,
        index / probe_count_xy
    );
}

vec3 GridPositionToWorldPosition(uvec3 pos)
{
    vec3 half_border = vec3(probe_system.probe_border) * 0.5;
    
    return (vec3(pos) - half_border) * probe_system.probe_distance;
}

vec3 ProbeIndexToWorldPosition(uint index)
{
    return GridPositionToWorldPosition(ProbeIndexToGridPosition(index));
}

vec3 SphericalFibonacci(uint index, uint n)
{
    float i = float(index);
    
    const float PHI = sqrt(5) * 0.5 + 0.5;
#define madfrac(A, B) ((A) * (B)-floor((A) * (B)))
    float phi       = 2.0 * M_PI * madfrac(i, PHI - 1);
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / float(n));
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0f, 1.0f));

    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
#undef madfrac
}


#endif