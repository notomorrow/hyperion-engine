#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

#define M_PI 3.14159265359

#include "probe_uniforms.inc"

#define PROBE_GRID_ORIGIN (probe_system.aabb_min.xyz)
#define PROBE_GRID_STEP   (vec3(probe_system.probe_distance))
#define PROBE_TOTAL_COUNT (probe_system.probe_counts.x * probe_system.probe_counts.y * probe_system.probe_counts.z)
#define PROBE_NORMAL_BIAS (0.01)

#define PROBE_SIDE_LENGTH_DEPTH      16
#define PROBE_SIDE_LENGTH_IRRADIANCE 8

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
    
    //return (vec3(pos) - half_border) * probe_system.probe_distance;
    
    return vec3(probe_system.probe_distance) * (vec3(pos) - half_border) + probe_system.aabb_min.xyz;
}

uint GridPositionToProbeIndex(uvec3 pos)
{
    return pos.x
        + probe_system.probe_counts.x * pos.y
        + probe_system.probe_counts.x * probe_system.probe_counts.y * pos.z;
}


vec3 ProbeIndexToWorldPosition(uint index)
{
    return GridPositionToWorldPosition(ProbeIndexToGridPosition(index));
}

uvec3 BaseGridCoord(vec3 P)
{   
    return PROBE_TOTAL_COUNT == 0
        ? uvec3(0)
        : clamp(uvec3(max(vec3(0.0), P - PROBE_GRID_ORIGIN) / PROBE_GRID_STEP), uvec3(0), probe_system.probe_counts.xyz - uvec3(1));
}

vec2 EncodeOctahedralCoord(in vec3 v)
{
#define NON_ZERO_SIGN(n) (n >= 0.0 ? 1.0 : -1.0)
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 vec = v.xy * (1.0 / l1norm);

    if (v.z < 0.0) {
        vec = (1.0 - abs(vec.yx)) * vec2(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
    }
#undef NON_ZERO_SIGN

    return vec;
}

vec3 DecodeOctahedralCoord(vec2 coord)
{
#define NON_ZERO_SIGN(n) (n >= 0.0 ? 1.0 : -1.0)
    vec3 vec = vec3(coord.x, coord.y, 1.0 - abs(coord.x) - abs(coord.y));
    
    if (vec.z < 0.0) {
        vec.xy = (1.0 - abs(vec.yx)) * vec2(NON_ZERO_SIGN(vec.x), NON_ZERO_SIGN(vec.y));
    }
#undef NON_ZERO_SIGN
    
    return normalize(vec);
}

vec2 TextureCoordFromDirection(vec3 dir, uint probe_index, uvec2 image_dimensions, uint probe_side_length)
{
    vec2 normalizedOctCoord = EncodeOctahedralCoord(normalize(dir));
    vec2 normalizedOctCoordZeroOne = (normalizedOctCoord + vec2(1.0f)) * 0.5f;

    // Length of a probe side, plus one pixel on each edge for the border
    float probeWithBorderSide = float(probe_side_length) + 2.0f;

    vec2 octCoordNormalizedToTextureDimensions = (normalizedOctCoordZeroOne * float(probe_side_length)) / vec2(image_dimensions);

    uint probesPerRow = (image_dimensions.x - 2) / uint(probeWithBorderSide);

    // Add (2,2) back to texCoord within larger texture. Compensates for 1 pix 
    // border around texture and further 1 pix border around top left probe.
    vec2 probeTopLeftPosition = vec2(mod(probe_index, probesPerRow) * probeWithBorderSide,
        (probe_index / probesPerRow) * probeWithBorderSide) + vec2(2.0f, 2.0f);

    vec2 normalizedProbeTopLeftPosition = vec2(probeTopLeftPosition) / vec2(image_dimensions);

    return vec2(normalizedProbeTopLeftPosition + octCoordNormalizedToTextureDimensions);
}

vec3 SphericalFibonacci(uint index, uint n)
{
    float i = float(index);
    
    const float PHI = sqrt(5.0) * 0.5 + 0.5;
#define madfrac(A, B) ((A) * (B)-floor((A) * (B)))
    float phi       = 2.0 * M_PI * madfrac(i, PHI - 1);
    float cos_theta = 1.0 - (2.0 * i + 1.0) * (1.0 / float(n));
    float sin_theta = sqrt(clamp(1.0 - cos_theta * cos_theta, 0.0f, 1.0f));

    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
#undef madfrac
}


#endif
