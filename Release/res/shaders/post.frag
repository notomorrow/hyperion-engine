varying vec2 v_texcoord0;

uniform sampler2D u_texture;

void main()
{
  vec4 tex = texture2D(u_texture, v_texcoord0);

  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
