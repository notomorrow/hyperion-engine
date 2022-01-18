#version 330 core

#include "include/attributes.inc"

out vec4 v_position;
out vec4 v_positionCamspace;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec3 v_tangent;
out vec3 v_bitangent;
out mat3 v_tbn;

#if SKINNING
#include "include/skinning.inc"
#endif
#include "include/matrices.inc"

uniform int FlipUV_X;
uniform int FlipUV_Y;

void main() {
  vec3 n = a_normal.xyz;
  v_texcoord0.x = abs(float(FlipUV_X) - a_texcoord0.x);
  v_texcoord0.y = abs(float(FlipUV_Y) - a_texcoord0.y);

#if SKINNING
	mat4 skinningMat = createSkinningMatrix();
	
	v_position = u_modelMatrix * skinningMat * vec4(a_position, 1.0);
    v_positionCamspace = u_viewMatrix * u_modelMatrix * skinningMat * vec4(a_position, 1.0);
	v_normal = transpose(inverse(u_modelMatrix * skinningMat)) * vec4(a_normal, 0.0);
#endif
	
#if !SKINNING
  v_position = u_modelMatrix * vec4(a_position, 1.0);
  v_positionCamspace = u_viewMatrix * v_position;
  v_normal = transpose(inverse(u_modelMatrix)) * vec4(a_normal, 0.0);
  //v_tangent = (transpose(inverse(u_modelMatrix)) * vec4(a_tangent, 0.0)).xyz;
  //v_bitangent = (transpose(inverse(u_modelMatrix)) * vec4(a_bitangent, 0.0)).xyz;
#endif

	v_tangent = a_tangent - a_normal * dot( a_tangent, a_normal ); // orthonormalization ot the tangent vectors
	v_bitangent = a_bitangent - a_normal * dot( a_bitangent, a_normal ); // orthonormalization of the binormal vectors to the normal vector 
	v_bitangent = v_bitangent - v_tangent * dot( v_bitangent, v_tangent ); // orthonormalization of the binormal vectors to the tangent vector
	v_tbn = mat3( normalize(v_tangent), normalize(v_bitangent), a_normal );

  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}
