#include "defines.inc"

struct Object
{
    // 0
    mat4 model_matrix;
    
    // 64
    vec4 local_aabb_max;
    // 80
    vec4 local_aabb_min;
    // 96
    vec4 world_aabb_max;
    // 112
    vec4 world_aabb_min;

    // 128
    uint entity_id;
    uint scene_id;
    uint mesh_id;
    uint material_id;
    uint skeleton_id;

    uint bucket;
    uint _pad0, _pad1, _pad2;

    HYP_PAD_STRUCT_HERE(float, (256 - 168) / 4);
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(std430, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 1, row_major) readonly buffer ObjectBuffer
{
    Object object;
};

#endif