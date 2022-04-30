#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec2 v_texcoord0;
layout(location=2) out flat vec3 v_light_direction;
layout(location=3) out flat vec3 v_camera_position;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;




layout(std430, set = 2, binding = 0, row_major) uniform SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 light_direction;
} scene;

struct Skeleton {
    mat4 bones[128];
};


layout(std140, set = 3, binding = 2, row_major) readonly buffer SkeletonBuffer {
    Skeleton skeleton;
};

void main() {
    vec4 position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_texcoord0 = a_texcoord0;
    v_light_direction = scene.light_direction.xyz;
    v_camera_position = scene.camera_position.xyz;

    gl_Position = position;
} 