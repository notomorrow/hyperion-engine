#version 330 core

layout(location = 0) in vec3 position; // the vertex position
layout(location = 1) in vec3 offset; // the particle's position
layout(location = 2) in float lifespan; // opacity value

out vec4 v_position;
out vec2 v_texcoord0;
out float v_lifespan;

#include "include/matrices.inc"

void main()
{
  v_lifespan = lifespan;
  
  vec3 right = vec3(u_viewMatrix[0][0], u_viewMatrix[1][0], u_viewMatrix[2][0]);
  vec3 up = vec3(u_viewMatrix[0][1], u_viewMatrix[1][1], u_viewMatrix[2][1]);
  
  v_texcoord0 = vec2(position.x + 0.5, position.y + 0.5);
  v_position = vec4(offset + (right * position.x) + (up * position.y), 1.0);
  gl_Position = u_projMatrix * u_viewMatrix * v_position;
}