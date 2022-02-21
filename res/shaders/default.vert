#version 330 core

#include "include/attributes.inc"

out vec4 v_position;
out vec4 v_positionCamspace;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec3 v_tangent;
out vec3 v_bitangent;
out mat3 v_tbn;

uniform mat4 WorldToNdcMatrix;

#if SKINNING
#include "include/skinning.inc"
#endif
#include "include/matrices.inc"

uniform int FlipUV_X = 0;
uniform int FlipUV_Y = 0;
uniform vec2 UVScale = vec2(1.0);

vec3 calculate_tangent(vec3 n) {
	vec3 v = vec3(1.0, 0.0, 0.0);
	float d = dot(v, n);
	if (abs(d) < 1.0e-3) {
		v = vec3(0.0, 1.0, 0.0);
		d = dot(v, n);
	}
	return normalize(v - d * n);
}

void main() {
  vec3 n = a_normal.xyz;
  v_texcoord0.x = abs(float(FlipUV_X) - a_texcoord0.x) * UVScale.x;
  v_texcoord0.y = abs(float(FlipUV_Y) - a_texcoord0.y) * UVScale.y;
  
  mat4 normalMatrix;

#if SKINNING
	mat4 skinningMat = createSkinningMatrix();
	
	v_position = u_modelMatrix * skinningMat * vec4(a_position, 1.0);
    v_positionCamspace = u_viewMatrix * u_modelMatrix * skinningMat * vec4(a_position, 1.0);
	normalMatrix = transpose(inverse(u_modelMatrix * skinningMat));
#endif
	
#if !SKINNING
	v_position = u_modelMatrix * vec4(a_position, 1.0);
	v_positionCamspace = u_viewMatrix * v_position;
	normalMatrix = transpose(inverse(u_modelMatrix));
#endif

	v_normal = normalMatrix * vec4(a_normal, 0.0);
	
	v_tangent = a_tangent - a_normal * dot( a_tangent, a_normal ); // orthonormalization ot the tangent vectors
	v_bitangent = a_bitangent - a_normal * dot( a_bitangent, a_normal ); // orthonormalization of the binormal vectors to the normal vector 
	v_bitangent = v_bitangent - v_tangent * dot( v_bitangent, v_tangent ); // orthonormalization of the binormal vectors to the tangent vector
	v_tangent = (normalMatrix * vec4(v_tangent, 0.0)).xyz;
	v_bitangent = (normalMatrix * vec4(v_bitangent, 0.0)).xyz;
	v_tbn = mat3( normalize(v_tangent), normalize(v_bitangent), normalize(v_normal.xyz));

	gl_Position = u_projMatrix * u_viewMatrix * v_position;
}
