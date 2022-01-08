#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 2) in vec2 a_texcoord0;

out vec2 v_texcoord0;

void main()
{
  v_texcoord0 = a_texcoord0;
  gl_Position = vec4(a_position, 1.0);
}