#version 450

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=6) out vec3 v_light_direction;
layout(location=7) out vec3 v_camera_position;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

struct Object {
    mat4 model_matrix;
};

layout(std140, set = 2, binding = 0, row_major) uniform SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 light_direction;
} scene;

layout(std140, set = 3, binding = 1, row_major) readonly buffer ObjectBuffer {
    Object object;
};


//push constants block
layout( push_constant ) uniform constants
{
	vec4 data;
} PushConstants;

void main() {
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    mat4 normal_matrix = transpose(inverse(object.model_matrix));

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
    v_light_direction = scene.light_direction.xyz;
    v_camera_position = scene.camera_position.xyz;

    gl_Position = scene.projection * scene.view * position;
} 