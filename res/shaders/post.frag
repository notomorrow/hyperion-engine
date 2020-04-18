in vec2 v_texcoord0;

#include "include/frag_output.inc"

uniform sampler2D u_texture;

void main()
{
  vec4 tex = texture(u_texture, v_texcoord0);
  tex.a = 0.5;
  output0 = vec4(1.0, 0.0, 0.0, 1.0);
}
