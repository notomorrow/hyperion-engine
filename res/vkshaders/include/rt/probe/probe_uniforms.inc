#ifndef PROBE_UNIFORMS
#define PROBE_UNIFORMS

#include "../../defines.inc"

#define PARAM_PROBE_DISTANCE 0
#define PARAM_NUM_RAYS_PER_PROBE 1

struct ProbeSystemUniforms
{
    vec4 aabb_max;
    vec4 aabb_min;
    uvec4 probe_border;
    uvec4 probe_counts;
    uvec4 grid_dimensions;
    uvec4 image_dimensions;
    vec4 params; // probe distance, num rays per probe

    //float _pad0, _pad1, _pad2, _pad3;
};

struct ProbeRayData
{
    vec4 direction_depth;
    vec4 origin;
    vec4 normal;
    vec4 color;
};

#define PROBE_RAY_DATA_INDEX(coord) (coord.x + probe_system.grid_dimensions.x * coord.y)

#endif