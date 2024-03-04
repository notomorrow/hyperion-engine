
#version 450

#extension GL_GOOGLE_include_directive : require

#include "Shared.glsl"

#define LOWRES_DITHER (PROBE_SIDE_LENGTH / PROBE_SIDE_LENGTH_LOWRES)

#ifdef MODE_OFFSET

#define WORKGROUP_SIZE 8
layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE, local_size_z = WORKGROUP_SIZE) in;

#else

#define WORKGROUP_SIZE 32
layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE) in;

#endif

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

HYP_DESCRIPTOR_CBUFF_DYNAMIC(VoxelizeProbeDescriptorSet, EnvGridBuffer, size = 4352) uniform EnvGridBuffer { EnvGrid env_grid; };

HYP_DESCRIPTOR_SSBO(VoxelizeProbeDescriptorSet, EnvProbesBuffer, size = 131072) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };

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

vec3 MapXYSToDirection(uint face_index, vec2 uv) {
    vec3 dir = vec3(0.0);

    float u = uv.x;
    float v = -uv.y;

    // +x, -x, +y, -y, +z, -z
    switch (face_index) {
    case 0:
        dir = normalize(vec3(1.0, v, -u));
        break;
    case 1:
        dir = normalize(vec3(-1.0, v, u));
        break;
    case 2:
        dir = normalize(vec3(u, 1.0, -v));
        break;
    case 3:
        dir = normalize(vec3(u, -1.0, v));
        break;
    case 4:
        dir = normalize(vec3(u, v, 1.0));
        break;
    case 5:
        dir = normalize(vec3(-u, v, -1.0));
        break;
    }

    return dir;
}

vec2 NormalizeOctahedralCoord(uvec2 coord)
{
    ivec2 oct_coord = ivec2(coord) % ivec2(cubemap_dimensions.xy);
    
    return (vec2(oct_coord) + vec2(0.5)) * (2.0 / vec2(cubemap_dimensions.xy)) - vec2(1.0);
}

#ifdef MODE_OFFSET

void DoPixel(uint _unused, uvec3 coord)
{
    // What is the size of one probe in the voxel grid?
    const ivec3 size_of_probe_in_voxel_grid = ivec3(voxel_texture_dimensions) / ivec3(env_grid.density.xyz);

    // Copy pixels at the current position over to the offset position
    const ivec3 current_position = ivec3(coord);
    const ivec3 offset_position = imod(current_position + (offset.xyz * size_of_probe_in_voxel_grid), ivec3(voxel_texture_dimensions));

    const vec4 current_pixel = imageLoad(voxel_grid_image, current_position);

    imageStore(voxel_grid_image, offset_position, current_pixel);
}

#else

void DoPixel(uint probe_index, uvec3 coord)
{
    EnvProbe env_probe = env_probes[probe_index % HYP_MAX_ENV_PROBES];

    ivec3 voxel_storage_position;

    vec3 dir = normalize(DecodeOctahedralCoord(NormalizeOctahedralCoord(coord.xy)));
    float depth_sample = TextureCube(sampler_nearest, depth_texture, dir).r;

    vec3 probe_world_position = (env_probe.aabb_max.xyz + env_probe.aabb_min.xyz) * 0.5;
    vec3 point_world_position = world_position.xyz + dir * depth_sample;

    // Voxel grid aabb must be 1:1:1 cube
    vec3 voxel_grid_aabb_min = vec3(min(env_grid.voxel_grid_aabb_min.x, min(env_grid.voxel_grid_aabb_min.y, env_grid.voxel_grid_aabb_min.z)));
    vec3 voxel_grid_aabb_max = vec3(max(env_grid.voxel_grid_aabb_max.x, max(env_grid.voxel_grid_aabb_max.y, env_grid.voxel_grid_aabb_max.z)));
    vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    vec3 scaled_position = (point_world_position - voxel_grid_aabb_center) / voxel_grid_aabb_extent;
    voxel_storage_position = ivec3(((scaled_position + 0.5) * vec3(voxel_texture_dimensions.xyz)));

    if (voxel_storage_position.x < 0 || voxel_storage_position.x >= voxel_texture_dimensions.x ||
        voxel_storage_position.y < 0 || voxel_storage_position.y >= voxel_texture_dimensions.y ||
        voxel_storage_position.z < 0 || voxel_storage_position.z >= voxel_texture_dimensions.z)
    {
        return;
    }

#ifdef MODE_VOXELIZE
    vec4 color_sample = TextureCube(sampler_nearest, color_texture, dir);

    imageStore(voxel_grid_image, voxel_storage_position, color_sample);//vec4(UINT_TO_VEC4(probe_grid_position.w).rgb, 1.0));
#endif
}

#endif

void main(void)
{
    const uint probe_index = probe_grid_position.w;

    DoPixel(probe_index, gl_GlobalInvocationID.xyz);
}