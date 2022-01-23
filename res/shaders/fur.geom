#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 87) out;

#include "include/matrices.inc"

#define $FUR_LAYERS 29
#define $FUR_LENGTH 0.85

in vec3 v_g_normal[3];
in vec2 v_g_texCoord[3];

out vec3 v_normal;
out vec4 v_position;
out vec2 v_texcoord0;

out float v_furStrength;

void main(void)
{
	vec3 normal;

	const float FUR_DELTA = 1.0 / float($FUR_LAYERS);
	
	float d = 0.0;

	for (int furLayer = 0; furLayer < $FUR_LAYERS; furLayer++)
	{
		d += FUR_DELTA;
		
		for(int i = 0; i < gl_in.length(); i++)
		{
			normal = normalize(v_g_normal[i]);
			v_normal = mat3(transpose(inverse(u_modelMatrix))) * normal; 

			v_texcoord0 = v_g_texCoord[i];
			
			// If the distance of the layer is getting bigger to the original surface, the layer gets more transparent.   
			v_furStrength = 1.0 - d;
			
			vec3 gravity = vec3(0.0, 0.0, 0.0);//(u_modelMatrix*vec4(0.0, 1.0, 0.0, 0.0)).xyz;
			v_position = u_modelMatrix * (gl_in[i].gl_Position + vec4((normal + gravity) * d * $FUR_LENGTH , 0.0));
			// Displace a layer along the surface normal.
			gl_Position = u_projMatrix * u_viewMatrix * v_position;
	
			EmitVertex();
		}
		
		EndPrimitive();
	}
}
