#version 150

varying vec2 v_texcoord0;
uniform sampler2D u_colorMap;

const float gamma = 2.2;

void main()
{
  vec4 tex = texture2D(u_colorMap, v_texcoord0);
  gl_FragColor.rgb = pow(tex.rgb, vec3(1.0/gamma));
}
