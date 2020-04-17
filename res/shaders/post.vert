attribute vec3 a_position;
attribute vec2 a_texcoord0;

varying vec2 v_texcoord0;

void main()
{
  v_texcoord0 = a_texcoord0;
  gl_Position = vec4(a_position, 1.0);
}