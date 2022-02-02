#version 420

#include "../include/attributes.inc"

#define $SCALE 5.0

uniform mat4 u_modelMatrix;
uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 WorldToNdcMatrix;

out vec4 v_position;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec4 ndcPos;

#if VCT_GEOMETRY_SHADER
out VSOutput
{
	vec4 position;
	vec3 normal;
	vec3 texcoord0;
} vs_out;
#endif

void main() 
{
    v_position = u_modelMatrix * vec4(a_position, 1.0);
    v_normal = transpose(inverse(u_modelMatrix)) * vec4(a_normal, 0.0);
	v_texcoord0 = a_texcoord0;
    ndcPos = v_position;;//u_projMatrix*u_viewMatrix*u_modelMatrix;//WorldToNdcMatrix * v_position;
	
#if VCT_GEOMETRY_SHADER
	vs_out.texcoord0 = vec3(a_texcoord0, 0.0);
	vs_out.normal = vec3(a_normal.xyz);
	vs_out.position = u_modelMatrix * vec4(a_position, 1.0);
#endif

	//gl_Position = vec4(a_position, 1.0);
	gl_Position = vec4(a_position, 1.0);
}
