#version 330 core

#include "include/attributes.inc"

out vec3 v_g_normal;
out vec2 v_g_texCoord;

void main()
{
	v_g_normal = a_normal;
	v_g_texCoord = a_texcoord0;

	gl_Position = vec4(a_position, 1.0);
}    
