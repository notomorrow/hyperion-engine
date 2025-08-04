#ifndef PROBE_COMMON_GLSL
#define PROBE_COMMON_GLSL

#include "../../Octahedron.glsl"

#define M_PI 3.14159265359

#define PROBE_GRID_ORIGIN (ddgiUniforms.aabb_min.xyz)
#define PROBE_GRID_STEP   (vec3(ddgiUniforms.probe_distance))
#define PROBE_TOTAL_COUNT (ddgiUniforms.probe_counts.x * ddgiUniforms.probe_counts.y * ddgiUniforms.probe_counts.z)
#define PROBE_NORMAL_BIAS (0.01)

#define PROBE_SIDE_LENGTH_DEPTH 16
#define PROBE_SIDE_LENGTH_IRRADIANCE 8
#define PROBE_BORDER_SIZE 2

ivec3 ProbeIndexToGridPosition(uint index)
{
    const int probe_count_xy = int(ddgiUniforms.probe_counts.x) * int(ddgiUniforms.probe_counts.y);
    
    return ivec3(
        index % int(ddgiUniforms.probe_counts.x),
        (index % probe_count_xy) / int(ddgiUniforms.probe_counts.x),
        index / probe_count_xy
    );
}

vec3 GridPositionToWorldPosition(ivec3 pos)
{
    //vec3 half_border = vec3(ddgiUniforms.probe_border) * 0.5;
    return PROBE_GRID_STEP * vec3(pos) + ddgiUniforms.aabb_min.xyz;
}

int GridPositionToProbeIndex(ivec3 pos)
{
    return pos.x
        + int(ddgiUniforms.probe_counts.x) * pos.y
        + int(ddgiUniforms.probe_counts.x) * int(ddgiUniforms.probe_counts.y) * pos.z;
}


vec3 ProbeIndexToWorldPosition(uint index)
{
    return GridPositionToWorldPosition(ProbeIndexToGridPosition(index));
}

ivec3 BaseGridCoord(vec3 P)
{   
    return PROBE_TOTAL_COUNT == 0
        ? ivec3(0)
        : clamp(ivec3(max(vec3(0.0), P - PROBE_GRID_ORIGIN) / PROBE_GRID_STEP), ivec3(0), ivec3(ddgiUniforms.probe_counts.xyz) - ivec3(1));
}

uvec2 DDGIGetProbeTexelCoords(uint probeIndex)
{
    // Find the probe's plane index
    uint probesPerPlane = (ddgiUniforms.probe_counts.x * ddgiUniforms.probe_counts.z);
    uint planeIndex = probeIndex / probesPerPlane;

    uint gridSpaceX = (probeIndex % ddgiUniforms.probe_counts.x);
    uint gridSpaceY = (probeIndex / ddgiUniforms.probe_counts.x);

    uint x = gridSpaceX + (planeIndex * ddgiUniforms.probe_counts.x);
    uint y = gridSpaceY % ddgiUniforms.probe_counts.z;

    return uvec2(x, y);
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
