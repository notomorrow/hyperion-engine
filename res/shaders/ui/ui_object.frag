#version 330 core

in vec2 v_texcoord0;

#include "../include/frag_output.inc"

uniform sampler2D ColorMap;
uniform int HasColorMap;

void main()
{
  vec4 color = vec4(1.0, 1.0, 1.0, 1.0);

  if (HasColorMap == 1) {
    color = texture(ColorMap, v_texcoord0);
  }

  output0 = color;
}
