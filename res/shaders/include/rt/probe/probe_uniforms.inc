#ifndef PROBE_UNIFORMS
#define PROBE_UNIFORMS

#include "../../defines.inc"

#define PARAM_PROBE_DISTANCE 0
#define PARAM_NUM_RAYS_PER_PROBE 1
#define PARAM_FLAGS 2
#define PARAM_NUM_BOUND_LIGHTS 3

#define PROBE_SYSTEM_FLAGS_NONE 0x0
#define PROBE_SYSTEM_FLAGS_FIRST_RUN 0x1

struct ProbeSystemUniforms
{
    vec4 aabb_max;
    vec4 aabb_min;
    uvec4 probe_border;
    uvec4 probe_counts;
    uvec4 grid_dimensions;
    uvec4 image_dimensions;
    float probe_distance;
    uint num_rays_per_probe;
    uint flags;
    uint num_bound_lights;

    uint shadow_map_index;
    uint _pad0, _pad1, _pad2;

    uvec4 light_indices[4];

    //uvec4 params; // x = probe distance, y = num rays per probe, z = flags, w =  num bound lights

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