#version 330

attribute vec3 a_position;
varying vec4 v_position;

#include "include/matrices.inc"

void main() {
  v_position = u_modelMatrix * vec4(a_position, 1.0);
  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}