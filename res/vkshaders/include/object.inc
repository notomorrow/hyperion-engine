#include "defines.inc"

struct Object {
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

    /*vec4 _pad0;
    vec4 _pad1;
    vec4 _pad2;
    vec4 _pad3;
    // 228
    vec4 _pad4;
    vec4 _pad5;
    float _pad6;
    float _pad7;
    float _pad8;*/
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

layout(std430, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 1, row_major) readonly buffer ObjectBuffer {
    Object object;
};

#endif