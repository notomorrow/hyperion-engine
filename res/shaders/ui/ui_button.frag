#version 330 core

in vec2 v_texcoord0;
in vec3 v_position;

#include "../include/frag_output.inc"

uniform sampler2D ColorMap;
uniform int HasColorMap;

void main()
{
  vec4 colorTop = vec4(0.565, 1.0, 0.78, 1.0);
  vec4 colorBottom = vec4(0.066, 0.357, 0.388, 1.0);

  float mixValue = 1.0 - v_texcoord0.y;
  mixValue = sqrt(mixValue);
  vec4 color = mix(colorTop, colorBottom, mixValue);


  vec2 pos = (vec2(1.0) - v_texcoord0);

  float vignette = pos.x * pos.y * (1.0-pos.x) * (1.0-pos.y);

  color.a = color.a * smoothstep(0, pow(0.2, 4.0), vignette);

  if (color.a < 0.2) {
    discard;
  }

  output0 = color;
}
