#version 330 core

#include "include/attributes.inc"

out vec4 v_position;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec3 v_tangent;
out vec3 v_bitangent;

#include "include/matrices.inc"

void main() {
  v_texcoord0 = a_texcoord0;

  v_position = u_modelMatrix * vec4(a_position, 1.0);
  v_normal = transpose(inverse(u_modelMatrix)) * vec4(a_normal, 0.0);

  v_tangent = a_tangent - a_normal * dot( a_tangent, a_normal ); // orthonormalization ot the tangent vectors
  v_bitangent = a_bitangent - a_normal * dot( a_bitangent, a_normal ); // orthonormalization of the binormal vectors to the normal vector
  v_bitangent = v_bitangent - v_tangent * dot( v_bitangent, v_tangent ); // orthonormalization of the binormal vectors to the tangent vector

  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}
