#ifndef VCT_SHARED_GLSL
#define VCT_SHARED_GLSL

#include "uniforms.inc"

vec3 vct_aabb_center = (vct_params.aabb_max.xyz + vct_params.aabb_min.xyz) * 0.5;
vec3 vct_aabb_extent = vct_params.aabb_max.xyz - vct_params.aabb_min.xyz;
uint vct_greatest_extent = max(vct_params.extent.x, max(vct_params.extent.y, vct_params.extent.z));

#define VOXEL_GRID_SCALE 1.0

// TODO: have to review these, we might be losing 50% of data
vec3 VctStoragePosition(vec3 scaled_position)
{
    const vec3 image_size = vec3(vct_params.extent.xyz);

    return vec3((scaled_position * 0.5 + 0.5) * image_size);
}

vec3 VctAabbToWorld(vec3 scaled_position)
{
    return ((scaled_position * 2.0 - 1.0) * vct_aabb_extent) + vct_aabb_center;
}

vec3 VctWorldToAabb(vec3 world_position)
{
    return ((world_position - vct_aabb_center) / vct_aabb_extent);
}

#endif
