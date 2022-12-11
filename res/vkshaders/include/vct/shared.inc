#ifndef VCT_SHARED_GLSL
#define VCT_SHARED_GLSL

#include "uniforms.inc"

#define HYP_VCT_MODE_TEXTURE 1
#define HYP_VCT_MODE_SVO 2

#ifndef HYP_VCT_MODE
    #ifdef VCT_ENABLED_SVO
        #define HYP_VCT_MODE HYP_VCT_MODE_SVO
    #elif defined(VCT_ENABLED_TEXTURE)
        #define HYP_VCT_MODE HYP_VCT_MODE_TEXTURE
    #else
        #error No VCT mode defined
    #endif
#endif

vec3 vct_aabb_center = (vct_params.aabb_max.xyz + vct_params.aabb_min.xyz) * 0.5;
vec3 vct_aabb_extent = vct_params.aabb_max.xyz - vct_params.aabb_min.xyz;
float vct_aabb_greatest_extent = max(vct_aabb_extent.x, max(vct_aabb_extent.y, vct_aabb_extent.z));
uint vct_greatest_extent = max(vct_params.extent.x, max(vct_params.extent.y, vct_params.extent.z));

#define VOXEL_GRID_SCALE 1.0

vec3 VctStoragePosition(vec3 scaled_position)
{
    const vec3 image_size = vec3(vct_params.extent.xyz);

    return vec3((scaled_position * 0.5 + 0.5) * (image_size - vec3(1.0)));
}

vec3 VctAABBToWorld(vec3 scaled_position)
{
    return ((scaled_position * 2.0 - 1.0) * vct_aabb_extent) + vct_aabb_center;
}

vec3 VctWorldToAABB(vec3 world_position)
{
    return ((world_position - vct_aabb_center) / vct_aabb_extent);
}


// SVO Methods

vec3 ScaleToAABB(vec3 aabb_max, vec3 aabb_min, vec3 world_position)
{
    vec3 extent3 = aabb_max - aabb_min;
    float extent = max(extent3.x, max(extent3.y, extent3.z)) * 0.5;
    float inv_extent = 1.0 / extent;
    vec3 center = (aabb_max + aabb_min) * 0.5;
    
    return (world_position - center) * inv_extent;
}

vec3 OffsetVoxelPosition(vec3 voxel_position)
{
    return (voxel_position + vec3(1.0)) * vec3(0.5);
}

vec3 ToVoxelPosition(vec3 aabb_max, vec3 aabb_min, vec3 world_position, float grid_size)
{
    vec3 voxel_pos = OffsetVoxelPosition(ScaleToAABB(aabb_max, aabb_min, world_position));
    
    return clamp(vec3(voxel_pos * grid_size), vec3(0.0), vec3(grid_size - 1.0));
}

vec4 RGBA8ToVec4(uint val)
{
	return vec4(float(val & 0xffu), float((val >> 8u) & 0xffu), float((val >> 16u) & 0xffu), float((val >> 24u) & 0x3fu));
}

uint Vec4ToRGBA8(vec4 vec)
{
	return (min(uint(vec.w), 0x3fu) << 24u) | (uint(vec.x) & 0xffu) | ((uint(vec.y) & 0xffu) << 8u) | ((uint(vec.z) & 0xffu) << 16u) |
	       0xC0000000u;
}

uvec4 RGBA8ToUvec4(uint val)
{
	return uvec4((val & 0xffu), ((val >> 8u) & 0xffu), ((val >> 16u) & 0xffu), ((val >> 24u) & 0x3fu));
}

uint Uvec4ToRGBA8(uvec4 vec)
{
	return (min(vec.w, 0x3fu) << 24u) | (vec.x & 0xffu) | ((vec.y & 0xffu) << 8u) | ((vec.z & 0xffu) << 16u) |
	       0xC0000000u;
}


vec4 RGBA8ToVec4Raw(uint val)
{
	return vec4(
	 	float(( val & 0x000000FF )),
	 	float(( val & 0x0000FF00 ) >> 8U ),
	 	float(( val & 0x00FF0000 ) >> 16U),
	 	float(( val & 0xFF000000 ) >> 24U) 
	);
}

uint Vec4ToRGBA8Raw(vec4 val)
{
	return (uint(val.w) & 0x000000FF) << 24U | 
           (uint(val.z) & 0x000000FF) << 16U | 
           (uint(val.y) & 0x000000FF) << 8U | 
           (uint(val.x) & 0x000000FF);
}



#endif
