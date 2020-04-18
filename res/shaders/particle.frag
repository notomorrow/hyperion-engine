#version 330 core

in vec4 v_position;
in vec2 v_texcoord0;
in float v_lifespan;

#include "include/frag_output.inc"

#if DIFFUSE_MAP
uniform sampler2D u_diffuseTexture;
#endif

void main()
{
  vec4 diffuseTexture = vec4(1.0, 1.0, 1.0, 1.0);
#if DIFFUSE_MAP
  diffuseTexture = texture(u_diffuseTexture, v_texcoord0);
#endif
  vec4 color = diffuseTexture;
  color.a *= v_lifespan;
  output0 = color;
}