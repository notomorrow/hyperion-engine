
#version 330 core
precision highp float;

uniform sampler2D u_colorMap;
uniform sampler2D u_normalMap;
uniform sampler2D u_positionMap;
uniform sampler2D u_depthMap;

uniform vec3 u_kernel[$KERNEL_SIZE];

uniform sampler2D u_noiseMap;

uniform vec2 u_noiseScale;
uniform mat4 u_view;
uniform mat4 u_projectionMatrix;

uniform float u_radius;

varying vec2 v_texcoord0;

uniform mat4 u_inverseProjectionMatrix;

void main(void)
{
	gl_FragColor = vec4(texture(u_colorMap, v_texcoord0).rgb, 1.0);
}
