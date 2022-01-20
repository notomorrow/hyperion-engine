#version 420

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec4 ndcPos;


#include "../include/matrices.inc"
#include "../include/frag_output.inc"

in vec3 v_normal;
layout(binding = 0, rgba32f) uniform image3D framebufferImage;
uniform vec4 C_albedo;
uniform vec3 u_probePos;

uniform float u_intensity;

// output
in GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
} gs_out;
vec3 worldToTex(vec3 world) 
{
	vec4 ss = u_projMatrix * vec4(world, 1.0);
	ss /= ss.w;
	ss.z = 0.0;
	
	return ss.xyz;

}
void main(void) 
{
	float voxelImageSize = 128.0;
	float halfVoxelImageSize = voxelImageSize * 0.5;

	
	
	vec4 imageColor = vec4(1.0, 0.0, 0.0, 1.0);
	//imageColor *= u_intensity;
	//imageColor *= lighting;
	//imageStore(framebufferImage, ivec3(gl_FragCoord.x/8, gl_FragCoord.y/8, gl_FragCoord.z/8), C_albedo);
	imageStore(framebufferImage, ivec3(ndcPos.x, ndcPos.y, ndcPos.z)-ivec3(u_probePos.x, u_probePos.y, u_probePos.z)+ivec3(halfVoxelImageSize), imageColor);
	
}
