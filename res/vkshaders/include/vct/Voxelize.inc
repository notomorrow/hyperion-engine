#ifndef HYP_VOXELIZE_GLSL
#define HYP_VOXELIZE_GLSL

#define HYP_VCT_MODE_TEXTURE 1
#define HYP_VCT_MODE_SVO 2

#ifndef HYP_VCT_MODE
    #error No VCT mode defined
#endif

#if HYP_VCT_MODE == HYP_VCT_MODE_SVO

uint CreateVoxelID()
{
    return atomicAdd(counter, 1u);
}

void WriteVoxel(uint voxel_id, vec3 position_aabb_space, vec4 color)
{
    position_aabb_space *= 0.5;
    position_aabb_space += 0.5;

    uint voxel_color = Vec4ToRGBA8Raw(color * 255.0);
    
    uvec3 position_grid_space = clamp(uvec3(position_aabb_space * grid_size), uvec3(0u), uvec3(grid_size - 1u));
    fragments[voxel_id].x = position_grid_space.x | (position_grid_space.y << 12u) | ((position_grid_space.z & 0xffu) << 24u); // only store last 8 bits of Z
    fragments[voxel_id].y = ((position_grid_space.z >> 8u) << 28u) | (voxel_color & 0x00ffffffu);
}

#elif HYP_VCT_MODE == HYP_VCT_MODE_TEXTURE

void WriteVoxel(vec3 position_aabb_space, vec4 color)
{
    imageStore(voxel_image, clamp(ivec3(VctStoragePosition(g_voxel)), ivec3(0), ivec3(vct_params.extent.xyz)), color);
}

#else

#error Invalid VCT Mode

#endif

#endif