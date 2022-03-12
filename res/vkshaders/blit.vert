#version 450

layout(location=0) out vec3 v_position;
layout(location=1) out vec2 v_texcoord0;
layout(location=2) out vec3 v_light_direction;
layout(location=3) out vec3 v_camera_position;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;

layout(set = 0, binding = 1, row_major) uniform SceneDataBlock {
    vec3 camera_position;
    vec3 light_direction;
} SceneData;

void main() {
    vec4 position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_texcoord0 = a_texcoord0;
    v_light_direction = SceneData.light_direction;
    v_camera_position = SceneData.camera_position;

    gl_Position = position;
} 