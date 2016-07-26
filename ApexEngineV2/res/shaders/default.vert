attribute vec3 a_position;
attribute vec2 a_texcoord0;

varying vec3 v_position;
varying vec2 v_texcoord0;

#include "include/matrices.inc"

void main() { 
	v_position = a_position;
	v_texcoord0 = a_texcoord0;
	gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(v_position, 1.0); 
}