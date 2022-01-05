#version 330 core

#include "include/attributes.inc"

out vec4 v_position;
out vec4 v_positionCamspace;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec3 v_tangent;
out vec3 v_bitangent;
out mat3 v_tbn;

#include "include/matrices.inc"

void main() {
	v_position = vec4(a_position, 1.0);
  v_texcoord0 = a_texcoord0;

  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * v_position;
}
