#version 330

layout(location = 0) in vec3 position; // the vertex position
layout(location = 1) in vec3 offset; // the particle's position
layout(location = 2) in float lifespan; // opacity value
varying vec4 v_position;
varying float v_lifespan;

#include "include/matrices.inc"

void main()
{
  v_lifespan = lifespan;
  
  vec3 particlePos = position + offset;
  vec3 pos = particlePos;
  v_position = vec4(pos, 1.0);
  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}