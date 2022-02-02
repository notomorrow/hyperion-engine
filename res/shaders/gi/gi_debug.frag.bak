#version 430

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.08

in vec4 v_position;
in vec4 v_ndc;
in vec4 v_positionCamspace;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "../include/matrices.inc"
#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/lighting.inc"
#include "../include/parallax.inc"

#if SHADOWS
#include "../include/shadows.inc"
#endif
uniform vec3 u_probePos;
uniform float u_scale;
uniform sampler3D tex[6];
layout(binding = 0, rgba32f) uniform image3D framebufferImage;
layout(binding = 1, rgba32f) uniform image3D framebufferImage1;
layout(binding = 2, rgba32f) uniform image3D framebufferImage2;
layout(binding = 3, rgba32f) uniform image3D framebufferImage3;
layout(binding = 4, rgba32f) uniform image3D framebufferImage4;
layout(binding = 5, rgba32f) uniform image3D framebufferImage5;

uniform mat4 WorldToVoxelTexMatrix;
// output
in GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
	vec4 position;
} gs_out;

void main() 
{
	output0 = u_diffuseColor;
	
	if (HasDiffuseMap == 1) {
	  output0 = texture(DiffuseMap, gs_out.texcoord0.xy);
	}
	output0 = min(output0, vec4(0.2));
}