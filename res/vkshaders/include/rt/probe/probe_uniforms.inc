
layout(std430, set = 9, binding = 9) uniform ProbeSystem {
    vec4  aabb_max;
    vec4  aabb_min;
    uvec4 probe_border;
    uvec4 probe_counts;
    float probe_distance;
    uint num_rays_per_probe;
} probe_system;

layout(push_constant) uniform constants {
    mat4 rotation;
    uint time;
} push_constants;