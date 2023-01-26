#ifndef HYP_VOXELIZE_GLSL
#define HYP_VOXELIZE_GLSL

#define HYP_VCT_MODE_TEXTURE 1
#define HYP_VCT_MODE_SVO 2

#if HYP_VCT_MODE == HYP_VCT_MODE_SVO
    layout(std140, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 72) buffer AtomicCounter
    {
        uint counter;
    };

    struct Fragment { uint x; uint y; };

    layout(std430, set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 73) writeonly buffer FragmentListBuffer
    {
        Fragment fragments[];
    };

    layout(push_constant) uniform Constants
    {
        uint grid_size;
        uint count_mode;
    };

    uint CreateVoxelID()
    {
        return atomicAdd(counter, 1u);
    }

    void WriteVoxel(uint voxel_id, vec3 position_aabb_space, vec4 color)
    {
        uint voxel_color = Vec4ToRGBA8Raw(color * 255.0);
        
        uvec3 position_grid_space = clamp(uvec3(position_aabb_space * grid_size), uvec3(0u), uvec3(grid_size - 1u));
        fragments[voxel_id].x = position_grid_space.x | (position_grid_space.y << 12u) | ((position_grid_space.z & 0xffu) << 24u); // only store last 8 bits of Z
        fragments[voxel_id].y = ((position_grid_space.z >> 8u) << 28u) | (voxel_color & 0x00ffffffu);
    }
#elif HYP_VCT_MODE == HYP_VCT_MODE_TEXTURE
    void WriteVoxel(vec3 position_aabb_space, vec4 color)
    {
        imageStore(voxel_image, ivec3(VctStoragePosition(g_voxel)), color);
    }
#else
    #error Invalid VCT Mode
#endif

#endif