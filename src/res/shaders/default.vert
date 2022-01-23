#version 330

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

#if SKINNING
#include "include/skinning.inc"
#endif
#include "include/matrices.inc"

void main() {
	v_texcoord0 = a_texcoord0;
	
#if SKINNING
	mat4 skinningMat = createSkinningMatrix();
	
	v_position = u_modelMatrix * skinningMat * vec4(a_position, 1.0);
	v_normal = transpose(inverse(u_modelMatrix * skinningMat)) * vec4(a_normal, 0.0);
  
	gl_Position = u_projMatrix * u_viewMatrix * v_position;
#endif
	
#if !SKINNING
  v_position = u_modelMatrix * vec4(a_position, 1.0);
  v_normal = transpose(inverse(u_modelMatrix)) * vec4(a_normal, 0.0);
    
  gl_Position = u_projMatrix * u_viewMatrix * v_position;
#endif
}
