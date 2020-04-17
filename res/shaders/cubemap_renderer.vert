#version 330

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;
attribute vec3 a_tangent;
attribute vec3 a_bitangent;

varying vec4 v_position;
varying vec4 v_positionCamspace;
varying vec4 v_normal;
varying vec2 v_texcoord0;
varying vec3 v_tangent;
varying vec3 v_bitangent;
varying mat3 v_tbn;

#include "include/matrices.inc"

void main() {
  gl_Position = u_modelMatrix * vec4(a_position, 1.0);
}