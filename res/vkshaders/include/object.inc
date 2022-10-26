#include "defines.inc"

struct Object
{
    mat4 model_matrix;
    mat4 previous_model_matrix;
    
    vec4 local_aabb_max;
    vec4 local_aabb_min;
    vec4 world_aabb_max;
    vec4 world_aabb_min;

    uint entity_id;
    uint scene_id;
    uint mesh_id;
    uint material_id;

    uint skeleton_id;
    uint bucket;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(std430, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 1, row_major) readonly buffer ObjectBuffer
{
    Object object;
};

#endif