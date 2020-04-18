#version 330 core

#include "../include/frag_output.inc"

in vec2 v_texcoord0;
uniform sampler2D u_colorMap;

const float gamma = 2.2;

void main()
{
  vec4 tex = texture(u_colorMap, v_texcoord0);
  output0 = vec4(vec3(pow(tex.rgb, vec3(1.0/gamma))), 1.0);
}
