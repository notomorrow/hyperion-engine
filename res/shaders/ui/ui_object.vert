#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 2) in vec2 a_texcoord0;

uniform vec2 Viewport;

#include "../include/matrices.inc"

out vec2 v_texcoord0;
out vec3 v_position;

void main()
{
  vec2 coord = (u_modelMatrix * vec4(a_position, 1.0)).xy / Viewport;
  coord.y = (1.0 - coord.y);
  coord.xy = coord.xy * 2.0 - 1.0;
  v_texcoord0 = a_texcoord0;
  v_position = a_position;
  gl_Position = u_modelMatrix * vec4(a_position, 1.0);
}
