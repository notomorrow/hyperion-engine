#ifndef VCT_SHARED_GLSL
#define VCT_SHARED_GLSL

#include "uniforms.inc"

vec3 vct_aabb_center = (vct_params.aabb_max.xyz + vct_params.aabb_min.xyz) * 0.5;
vec3 vct_aabb_extent = vct_params.aabb_max.xyz - vct_params.aabb_min.xyz;
uint vct_greatest_extent = max(vct_params.extent.x, max(vct_params.extent.y, vct_params.extent.z));

#define VOXEL_GRID_SCALE 1.0

vec3 VctStoragePosition(vec3 scaled_position)
{
    //const ivec3 half_voxel_image_size = ivec3(vct_params.extent.xyz) / 2;

    const vec3 image_size = vec3(vct_params.extent.xyz);
    const vec3 ratio      = image_size / vct_aabb_extent;

    return vec3((scaled_position * 0.5 + 0.5) * image_size);

	//return ivec3((scaled_position - vct_aabb_center) * VOXEL_GRID_SCALE) + half_voxel_image_size;
}

vec3 VctAabbToWorld(vec3 scaled_position)
{
    //float extent = max(vct_aabb_extent.x, max(vct_aabb_extent.y, vct_aabb_extent.z)) * 0.5;

    return ((scaled_position * 2.0 - 1.0) * vct_aabb_extent) + vct_aabb_center;
}

vec3 VctWorldToAabb(vec3 world_position)
{
    //float inv_extent = 1.0 / (max(vct_aabb_extent.x, max(vct_aabb_extent.y, vct_aabb_extent.z)) * 0.5);

    return ((world_position - vct_aabb_center) / vct_aabb_extent);
}

vec3 VctOffsetVoxelPosition(vec3 voxel_position)
{
    return (voxel_position + vec3(1.0)) * vec3(0.5);
}

#endif