
#version 330 core
precision highp float;

uniform sampler2D ColorMap;
uniform sampler2D NormalMap;
uniform sampler2D PositionMap;
uniform sampler2D DepthMap;

uniform vec3 u_kernel[$KERNEL_SIZE];

uniform sampler2D u_noiseMap;

uniform vec2 u_resolution;

uniform vec2 u_noiseScale;
uniform mat4 u_view;
uniform mat4 u_projectionMatrix;

uniform float u_radius;

in vec2 v_texcoord0;

uniform mat4 u_inverseProjectionMatrix;

#define $NOISE_AMOUNT 0.0002

#include "../include/frag_output.inc"

vec2 rand(vec2 coord) //generating noise/pattern texture for dithering
{
  float noiseX = ((fract(1.0-coord.s*(u_resolution.x/2.0))*0.25)+(fract(coord.t*(u_resolution.y/2.0))*0.75))*2.0-1.0;
  float noiseY = ((fract(1.0-coord.s*(u_resolution.x/2.0))*0.75)+(fract(coord.t*(u_resolution.y/2.0))*0.25))*2.0-1.0;

	noiseX = clamp(fract(sin(dot(coord ,vec2(12.9898,78.233))) * 43758.5453),0.0,1.0)*2.0-1.0;
	noiseY = clamp(fract(sin(dot(coord ,vec2(12.9898,78.233)*2.0)) * 43758.5453),0.0,1.0)*2.0-1.0;

  return vec2(noiseX,noiseY)*$NOISE_AMOUNT;
}

vec4 getViewPos(vec2 texCoord)
{
	// Calculate out of the fragment in screen space the view space position.

	float x = texCoord.s * 2.0 - 1.0;
	float y = texCoord.t * 2.0 - 1.0;

	// Assume we have a normal depth range between 0.0 and 1.0
	float z = texture(DepthMap, texCoord).r * 2.0 - 1.0;

	vec4 posProj = vec4(x, y, z, 1.0);

	vec4 posView = u_inverseProjectionMatrix * posProj;

	posView /= posView.w;

	return posView;
}

void main(void)
{
	// Calculate out of the current fragment in screen space the view space position.

	float scale = 0.25;

	vec4 posView = getViewPos(v_texcoord0);

	// Normal gathering.

	vec3 normalView = normalize(texture(NormalMap, v_texcoord0).xyz * 2.0 - 1.0);

	// Calculate the rotation matrix for the kernel.

	vec3 randomVector = normalize(texture(u_noiseMap, v_texcoord0 * u_noiseScale).xyz * 2.0 - 1.0);

	// Using Gram-Schmidt process to get an orthogonal vector to the normal vector.
	// The resulting tangent is on the same plane as the random and normal vector.
	// see http://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
	// Note: No division by <u,u> needed, as this is for normal vectors 1.
	vec3 tangentView = normalize(randomVector - dot(randomVector, normalView) * normalView);

	vec3 bitangentView = cross(normalView, tangentView);

	// Final matrix to reorient the kernel depending on the normal and the random vector.
	mat3 kernelMatrix = mat3(tangentView, bitangentView, normalView);

	// Go through the kernel samples and create occlusion factor.
	float occlusion = 0.0;

	for (int i = 0; i < int($KERNEL_SIZE); i++)
	{
		// Reorient sample vector in view space ...
		vec3 sampleVectorView = kernelMatrix * u_kernel[i] * scale;

		// ... and calculate sample point.
		vec4 samplePointView = posView + u_radius * vec4(sampleVectorView, 0.0);

		// Project point and calculate NDC.

		vec4 samplePointNDC = u_projectionMatrix * samplePointView;

		samplePointNDC /= samplePointNDC.w;

		// Create texture coordinate out of it.

		vec2 samplePointTexCoord = samplePointNDC.xy * 0.5 + 0.5;

		// Get sample out of depth texture

		float zSceneNDC = texture(DepthMap, samplePointTexCoord).r * 2.0 - 1.0;

		float delta = samplePointNDC.z - zSceneNDC;

		// If scene fragment is before (smaller in z) sample point, increase occlusion.
		if (delta > $CAP_MIN_DISTANCE && delta < $CAP_MAX_DISTANCE)
		{
			occlusion += 1.0;
		}
	}

    //vec3 noiseRgb = texture(u_noiseMap, v_texcoord0 * vec2(40.0)).rgb;
	// No occlusion gets white, full occlusion gets black.
	occlusion = min(max(1.0 - ((occlusion / float($KERNEL_SIZE))), 0.0), 1.0);

	output0 = vec4(texture(ColorMap, v_texcoord0).rgb * vec3(occlusion), 1.0);
}
