#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

uniform sampler2D u_diffuseTexture;

#include "include/shadows.inc"

void main() 
{
  vec3 shadowCoord = getShadowCoord(v_position.xyz);
  float shadowness = getShadow(shadowCoord);
  gl_FragColor = vec4(vec3(shadowness, shadowness, shadowness), 1.0); 
}