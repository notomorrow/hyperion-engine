
#version 450

#extension GL_GOOGLE_include_directive : require

#include "Shared.glsl"

#define LOWRES_DITHER (PROBE_SIDE_LENGTH / PROBE_SIDE_LENGTH_LOWRES)

#define WORKGROUP_SIZE 32
#define PIXELS_PER_WORKGROUP (PROBE_SIDE_LENGTH / WORKGROUP_SIZE)

layout(local_size_x = WORKGROUP_SIZE, local_size_y = WORKGROUP_SIZE) in;

#include "../include/defines.inc"
#include "../include/shared.inc"
#include "../include/tonemap.inc"
#include "../include/brdf.inc"
#include "../include/noise.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/env_probe.inc"

layout(set = 0, binding = 0) uniform textureCube color_texture;
layout(set = 0, binding = 1) uniform textureCube normals_texture;
layout(set = 0, binding = 2) uniform textureCube depth_texture;

layout(set = 0, binding = 3) uniform sampler sampler_linear;
layout(set = 0, binding = 4) uniform sampler sampler_nearest;

// #define USE_TEXTURE_ARRAY

layout(set = 0, binding = 10, rgba16f) uniform writeonly image3D voxel_grid_image;

layout(std140, set = 0, binding = 11) uniform EnvGridBuffer
{
    EnvGrid env_grid;
};

layout(std430, set = 0, binding = 12, row_major) readonly buffer EnvProbesBuffer
{
    EnvProbe env_probes[HYP_MAX_ENV_PROBES];
};

layout(push_constant) uniform PushConstant
{
    uvec4 probe_grid_position;
    uvec4 cubemap_dimensions;
    uvec4 probe_offset_coord;
    uvec4 probe_offset_coord_lowres;
    uvec4 grid_dimensions;
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
    ivec2 oct_frag_coord = ivec2((int(coord.x) - 2) % PROBE_SIDE_LENGTH_BORDER, (int(coord.y) - 2) % PROBE_SIDE_LENGTH_BORDER);
    
    return (vec2(oct_frag_coord) + vec2(0.5)) * (2.0 / float(PROBE_SIDE_LENGTH)) - vec2(1.0);
}

void DoPixel(uint probe_index, uvec2 coord)
{
    EnvProbe env_probe = env_probes[(probe_index + 1) % HYP_MAX_BOUND_LIGHT_FIELD_PROBES];

#ifndef USE_TEXTURE_ARRAY
    coord = probe_offset_coord.xy + uvec2(gl_GlobalInvocationID.xy) + 2;
#endif

    const vec3 dir = normalize(DecodeOctahedralCoord(NormalizeOctahedralCoord(coord)));

    const vec4 color_sample = TextureCube(sampler_nearest, color_texture, dir);
    const vec2 depth_sample = TextureCube(sampler_nearest, depth_texture, dir).rg;

    // const vec3 size_of_probe = env_grid.aabb_extent.xyz / vec3(env_grid.density.xyz);

    const vec3 point_world_position = env_probe.world_position.xyz + dir * depth_sample.r;

    // Voxel grid aabb must be 1:1:1 cube
    const vec3 voxel_grid_aabb_min = vec3(min(env_grid.aabb_min.x, min(env_grid.aabb_min.y, env_grid.aabb_min.z)));
    const vec3 voxel_grid_aabb_max = vec3(max(env_grid.aabb_max.x, max(env_grid.aabb_max.y, env_grid.aabb_max.z)));
    const vec3 voxel_grid_aabb_extent = voxel_grid_aabb_max - voxel_grid_aabb_min;
    const vec3 voxel_grid_aabb_center = voxel_grid_aabb_min + voxel_grid_aabb_extent * 0.5;

    const vec3 scaled_position = (point_world_position - voxel_grid_aabb_center) / voxel_grid_aabb_extent;
    const ivec3 voxel_storage_position = ivec3(((scaled_position * 0.5 + 0.5) * 256.0));

    if (voxel_storage_position.x < 0 || voxel_storage_position.x >= 256 ||
        voxel_storage_position.y < 0 || voxel_storage_position.y >= 256 ||
        voxel_storage_position.z < 0 || voxel_storage_position.z >= 256)
    {
        return;
    }

    imageStore(voxel_grid_image, voxel_storage_position, color_sample);
}

void main(void)
{
    const uint probe_index = probe_grid_position.w;

    DoPixel(probe_index, gl_GlobalInvocationID.xy);
}
