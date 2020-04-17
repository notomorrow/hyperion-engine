#version 330

#include "include/matrices.inc"

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_texcoord0;

void main(void)
{
  v_position = a_position;
  v_normal = a_normal;
  v_texcoord0 = a_texcoord0;
  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(a_position, 1.0);
}