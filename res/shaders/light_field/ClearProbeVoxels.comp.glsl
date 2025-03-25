
#version 450

#extension GL_GOOGLE_include_directive : require

#include "Shared.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/tonemap.inc"
#include "../include/brdf.inc"
#include "../include/noise.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/env_probe.inc"

HYP_DESCRIPTOR_SRV(VoxelizeProbeDescriptorSet, InColorImage) uniform textureCube color_texture;
HYP_DESCRIPTOR_SRV(VoxelizeProbeDescriptorSet, InNormalsImage) uniform textureCube normals_texture;
HYP_DESCRIPTOR_SRV(VoxelizeProbeDescriptorSet, InDepthImage) uniform textureCube depth_texture;

HYP_DESCRIPTOR_SAMPLER(VoxelizeProbeDescriptorSet, SamplerLinear) uniform sampler sampler_linear;
HYP_DESCRIPTOR_SAMPLER(VoxelizeProbeDescriptorSet, SamplerNearest) uniform sampler sampler_nearest;

HYP_DESCRIPTOR_UAV(VoxelizeProbeDescriptorSet, OutVoxelGridImage, format = rgba8) uniform image3D voxel_grid_image;

HYP_DESCRIPTOR_CBUFF_DYNAMIC(VoxelizeProbeDescriptorSet, EnvGridBuffer) uniform EnvGridBuffer { EnvGrid env_grid; };

HYP_DESCRIPTOR_SSBO(VoxelizeProbeDescriptorSet, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[]; };

layout(push_constant) uniform PushConstant
{
    uvec4 probe_grid_position;
    uvec4 voxel_texture_dimensions;
    uvec4 cubemap_dimensions;
#ifdef MODE_OFFSET
    ivec4 offset;
#else
    vec4  world_position;
#endif
};

void main(void)
{
    const uint probe_index = probe_grid_position.w;

    EnvProbe env_probe = env_probes[probe_index];

    const ivec3 size_of_probe_in_voxel_grid = ivec3(voxel_texture_dimensions) / ivec3(env_grid.density.xyz);

    if (any(greaterThanEqual(gl_GlobalInvocationID.xyz, size_of_probe_in_voxel_grid)))
    {
        return;
    }

    const ivec3 probe_voxel_grid_offset = ivec3(probe_grid_position.xyz) * size_of_probe_in_voxel_grid;

    vec3 voxel_grid_aabb_min = vec3(min(env_grid.voxel_grid_aabb_min.x, min(env_grid.voxel_grid_aabb_min.y, env_grid.voxel_grid_aabb_min.z)));
    vec3 voxel_grid_aabb_max = vec3(max(env_grid.voxel_grid_aabb_max.x, max(env_grid.voxel_grid_aabb_max.y, env_grid.voxel_grid_aabb_max.z)));
    vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    vec3 scaled_position = ((env_probe.world_position.xyz - voxel_grid_aabb_center) / voxel_grid_aabb_extent) + vec3(0.5);
    ivec3 voxel_storage_position = ivec3((scaled_position * vec3(voxel_texture_dimensions.xyz)) + vec3(gl_GlobalInvocationID.xyz));

    // ivec3 voxel_storage_position = ivec3(gl_GlobalInvocationID.xyz) + probe_voxel_grid_offset;

    if (voxel_storage_position.x < 0 || voxel_storage_position.x >= voxel_texture_dimensions.x ||
        voxel_storage_position.y < 0 || voxel_storage_position.y >= voxel_texture_dimensions.y ||
        voxel_storage_position.z < 0 || voxel_storage_position.z >= voxel_texture_dimensions.z)
    {
        return;
    }

    imageStore(voxel_grid_image, ivec3(voxel_storage_position), vec4(0.0));//vec4(UINT_TO_VEC4(probe_grid_position.w).rgb, 1.0));
}