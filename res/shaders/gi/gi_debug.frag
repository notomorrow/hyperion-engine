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

vec4 voxelFetch(vec3 pos, vec3 dir, float lod)
{
	vec4 sampleX =
		dir.x < 0.0
		? textureLod(tex[0], pos, lod)
		: textureLod(tex[1], pos, lod);
	
	vec4 sampleY =
		dir.y < 0.0
		? textureLod(tex[2], pos, lod)
		: textureLod(tex[3], pos, lod);
	
	vec4 sampleZ =
		dir.z < 0.0
		? textureLod(tex[4], pos, lod)
		: textureLod(tex[5], pos, lod);
	
	vec3 sampleWeights = abs(dir);
	float invSampleMag = 1.0 / (sampleWeights.x + sampleWeights.y + sampleWeights.z + .0001);
	sampleWeights *= invSampleMag;
	
	vec4 filtered = 
		sampleX * sampleWeights.x
		+ sampleY * sampleWeights.y
		+ sampleZ * sampleWeights.z;
		
		//if (dir.x < 0.0) {
		//	return textureLod(tex[0], pos, lod);
		//} else {
		//	return textureLod(tex[1], pos, lod);
		//}
		vec4 negX = textureLod(tex[0], pos, lod);
		vec4 posX = textureLod(tex[1], pos, lod);
		
		//if (dir.y < 0.0) {
		//	return textureLod(tex[2], pos, lod);
		//} else {
		///	return vec4(0.0);
		//}
		
		return mix(negX, posX, clamp(dir.x*0.5+0.5, 0.0, 1.0));
		
	
	return filtered;
}
// origin, dir, and maxDist are in texture space
// dir should be normalized
// coneRatio is the cone diameter to height ratio (2.0 for 90-degree cone)
vec4 voxelTraceCone(float minVoxelDiameter, vec3 origin, vec3 dir, float coneRatio, float maxDist)
{
	float minVoxelDiameterInv = 1.0/minVoxelDiameter;
	vec3 samplePos = origin;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(1.0, 1.0, 1.0, 1.0)*0.2;
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		vec3 samplePos = origin + dir * dist;
		vec4 sampleValue = voxelFetch(samplePos, -dir, sampleLOD);
		sampleValue = mix(sampleValue,fadeCol, clamp(dist/maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}
	return accum;
}
void main() 
{
	output0 = u_diffuseColor;
	
	if (HasDiffuseMap == 1) {
	  output0 = texture(DiffuseMap, gs_out.texcoord0.xy);
	}
	output0 = min(output0, vec4(0.2));
}