varying vec2 v_texcoord0;

uniform sampler2D u_texture;

void main()
{
  vec4 tex = texture2D(u_texture, v_texcoord0);
  tex.a = 0.5;
  gl_FragColor = tex;
}