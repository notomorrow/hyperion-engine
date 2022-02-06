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

uniform mat4 WorldToVoxelTexMatrix;
layout(binding = 0, rgba32f) uniform image3D framebufferImage;

#if VCT_GEOMETRY_SHADER
// output
in GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
	vec4 position;
} gs_out;

#endif

void main() 
{
#if VCT_GEOMETRY_SHADER
	vec4 position = gs_out.position;
    vec3 n = normalize(gs_out.normal);
#endif

#if !VCT_GEOMETRY_SHADER
	vec4 position = v_position;
    vec3 n = normalize(v_normal.xyz);
#endif
	
    vec3 viewVector = normalize(u_camerapos-position.xyz);
	float voxelImageSize = float($VCT_MAP_SIZE);
	float halfVoxelImageSize = voxelImageSize * 0.5;
	output0 = voxelFetch(decodeVoxelPosition(position.xyz), viewVector, 0);

	
	vec3 test_store_pos = (position.xyz - VoxelProbePosition) * $VCT_SCALE;
	//output0 = imageLoad(framebufferImage, ivec3(test_store_pos.x, test_store_pos.y, test_store_pos.z)+ivec3(halfVoxelImageSize));

}