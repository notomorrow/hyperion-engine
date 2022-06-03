struct Object {
    mat4 model_matrix;
    uint has_skinning;
    uint material_index;
    
    uvec2 _padding;
    
    vec4 local_aabb_max;
    vec4 local_aabb_min;
    vec4 world_aabb_max;
    vec4 world_aabb_min;
};

layout(std430, set = 3, binding = 1, row_major) readonly buffer ObjectBuffer {
    Object object;
};
