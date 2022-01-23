#version 330 core

#include "../include/frag_output.inc"
#include "../include/depth.inc"

#define $GI_AMOUNT 200.0

uniform sampler2D	ColorMap;
uniform sampler2D	NormalMap;
uniform sampler2D	DepthMap;
uniform mat4		InverseViewProjMatrix;
uniform vec2 u_resolution;

in vec2 v_texcoord0;

#define $KERNEL_SIZE 4

vec3 getLightPosition( vec2 uv, float d )
{
	vec4 pos_s = vec4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, d, 1.0f);
	vec4 pos_v = InverseViewProjMatrix * pos_s;
	return pos_v.xyz / pos_v.w;
}

vec3	getFragWorldPosition( vec2 coord, float lod )
{
	float depth = texture(DepthMap, coord).r;

	return positionFromDepth(InverseViewProjMatrix, coord, depth);
}

vec3	getFragNormal( vec2 coord, float lod )
{
    return texture(NormalMap, coord).rgb * 2.0 - 1.0;
}

float lenSq( vec3 vector )
{
	return vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
}

vec3 lightSample( vec2 coord, vec2 lightCoord, vec3 fragWPosition, vec3 fragWNormal )
{
	vec3 lightWPosition = getLightPosition(lightCoord, texture(DepthMap, coord).r);
	vec3 lightWNormal = getFragNormal(lightCoord, 8.0f).rgb;
	vec3 lightColor = texture(ColorMap, lightCoord).rgb;

	vec3 lightPath = lightWPosition - fragWPosition;
	vec3 lightDir = normalize(lightPath);

	float emitAmount = max(dot(lightDir, -lightWNormal), 0.0f);
	float receiveAmount = clamp(dot(lightDir, fragWNormal) * 0.5f + 0.5f, 0.0f, 1.0f);
	float distfall = pow(lenSq(lightPath), 0.1f) + 1.0f;

	return (lightColor * emitAmount * receiveAmount / distfall) / float($KERNEL_SIZE * $KERNEL_SIZE);
}

void main()
{
	vec3 direct = texture(ColorMap, v_texcoord0.st).rgb;
	vec3 fragWPosition = getFragWorldPosition(v_texcoord0.st, 0.0f);
	vec3 fragWNormal = getFragNormal(v_texcoord0.st, 0.0f);
	vec3 gi = vec3(0);

	float step = (1.0f / float($KERNEL_SIZE));
	vec2 s = vec2(step * 0.5, 1.0f - step * 0.5f);

	for (int i = 0; i < $KERNEL_SIZE; i++)
	{
		for (int j = 0; j < $KERNEL_SIZE; j++)
		{
			gi += lightSample(v_texcoord0.st, s, fragWPosition, fragWNormal);
			s.y -= step;
		}
		s.x += step;
		s.y = 1.0f - step * 0.5f;
	}

	vec4 albedo = vec4(direct + (gi / float($KERNEL_SIZE * $KERNEL_SIZE) * $GI_AMOUNT), 1.0);
	//if (v_texcoord0.st.x > 0.5)
	//	albedo = vec4(direct.rgb, 1.0f);

    output0 = vec4(direct, 1.0);
	output4 = vec4(albedo.rgb, 0.0);
}
