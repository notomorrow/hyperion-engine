#version 330

varying vec4 v_position;
varying vec2 v_texcoord0;
varying float v_lifespan;

#if DIFFUSE_MAP
uniform sampler2D u_diffuseTexture;
#endif

void main()
{
  vec4 diffuseTexture = vec4(1.0, 1.0, 1.0, 1.0);
#if DIFFUSE_MAP
  diffuseTexture = texture2D(u_diffuseTexture, v_texcoord0);
#endif
  vec4 color = diffuseTexture;
  color.a *= v_lifespan;
  gl_FragColor = color;
}