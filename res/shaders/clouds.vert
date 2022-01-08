#version 330 core

#include "include/attributes.inc"

#include "include/matrices.inc"

out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord0;

void main(void)
{
  v_position = a_position;
  v_normal = a_normal;
  v_texcoord0 = a_texcoord0;
  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(a_position, 1.0);
}