#ifndef PROBE_UNIFORMS
#define PROBE_UNIFORMS

layout(std430, set = 9, binding = 9) uniform ProbeSystem {
    vec4  aabb_max;
    vec4  aabb_min;
    uvec4 probe_border;
    uvec4 probe_counts;
    uvec2 image_dimensions;
    uvec2 irradiance_image_dimensions;
    uvec2 depth_image_dimensions;
    float probe_distance;
    uint  num_rays_per_probe;
} probe_system;

struct ProbeRayData {
    vec4 direction_depth;
    vec4 origin;
    vec4 normal;
    vec4 color;
};

layout(std430, set = 9, binding = 10) buffer ProbeRayDataBuffer {
    ProbeRayData probe_rays[];
};

#define PROBE_RAY_DATA_INDEX(coord) (coord.x + probe_system.image_dimensions.x * coord.y)

ProbeRayData GetProbeRayData(uvec2 coord)
{
    return probe_rays[PROBE_RAY_DATA_INDEX(coord)];
}

void SetProbeRayData(uvec2 coord, ProbeRayData ray_data)
{
    probe_rays[PROBE_RAY_DATA_INDEX(coord)] = ray_data;
}

#endif