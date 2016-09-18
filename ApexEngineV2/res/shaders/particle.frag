#version 330

varying vec4 v_position;
varying float v_lifespan;

void main()
{
  gl_FragColor = vec4(v_position.xyz, v_lifespan);
}