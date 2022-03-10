#version 450

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=6) out vec3 v_light_direction;
layout(location=7) out vec3 v_camera_position;
layout(location=8) out uint v_filter_frame_src;

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
	uint filter_frame_src;
} PushConstants;

void main() {
    vec4 position = vec4(a_position, 1.0);
    
    v_position = position.xyz;
    v_texcoord0 = a_texcoord0;
    
    v_light_direction = SceneData.light_direction;
    v_camera_position = SceneData.camera_position;
    v_filter_frame_src = filter_frame_src;

    gl_Position = position;
} 