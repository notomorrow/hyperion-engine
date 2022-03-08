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

layout(binding = 0, row_major) uniform MatrixBlock {
    mat4 model;
	mat4 view;
	mat4 projection;
} Matrix;

layout(binding = 1, row_major) uniform SceneDataBlock {
    vec3 camera_position;
    vec3 light_direction;
} SceneData;

//push constants block
layout( push_constant ) uniform constants
{
	vec4 data;
} PushConstants;

void main() {
    vec4 position = Matrix.model * vec4(a_position, 1.0);
    mat4 normal_matrix = transpose(inverse(Matrix.model));

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
    v_light_direction = SceneData.light_direction;
    v_camera_position = SceneData.camera_position;

    gl_Position = Matrix.projection * Matrix.view * position;
} 