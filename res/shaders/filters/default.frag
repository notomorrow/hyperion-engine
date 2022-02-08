#version 330 core

#include "../include/frag_output.inc"
#include "../include/depth.inc"

in vec2 v_texcoord0;
uniform sampler2D ColorMap;

void main()
{
  vec4 tex = texture(ColorMap, v_texcoord0);
  output0 = tex;
}
