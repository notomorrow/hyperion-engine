#include "defines.inc"

struct Object {
    mat4 model_matrix;
    uint has_skinning;
    uint entity_id;
    uint mesh_id;
    uint material_id;
    
    vec4 local_aabb_max;
    vec4 local_aabb_min;
    vec4 world_aabb_max;
    vec4 world_aabb_min;
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(std430, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 1, row_major) readonly buffer ObjectBuffer {
    Object object;
};

#endif