#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=6) in vec3 v_light_direction;
layout(location=7) in vec3 v_camera_position;

// TODO: read in from prev pass.

layout(location=0) out vec4 color_output;

//layout(binding = 2) uniform samplerCube tex;
//layout(binding = 2) uniform sampler2D tex;
layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
    color_output = vec4(texture(tex, vec2(v_texcoord0.x, 1.0 - v_texcoord0.y)).rgb * vec3(0.0, 0.0, 1.0), 1.0);
}