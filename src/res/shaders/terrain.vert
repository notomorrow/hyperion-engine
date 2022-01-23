#version 330

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;
attribute vec3 a_tangent;
attribute vec3 a_bitangent;

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;
varying vec3 v_tangent;
varying vec3 v_bitangent;

#include "include/matrices.inc"

void main() {
  v_texcoord0 = a_texcoord0;
	
  v_position = u_modelMatrix * vec4(a_position, 1.0);
  v_normal = transpose(inverse(u_modelMatrix)) * vec4(a_normal, 0.0);
  vec3 c1 = cross(a_normal, vec3(0.0, 0.0, 1.0));
  vec3 c2 = cross(a_normal, vec3(0.0, 1.0, 0.0));
  if (length(c1)>length(c2))
	v_tangent = c1;
  else
    v_tangent = c2;
  v_tangent = normalize(v_tangent);
  v_bitangent = cross(a_normal, v_tangent);
  v_bitangent = normalize(v_bitangent);
    
  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}