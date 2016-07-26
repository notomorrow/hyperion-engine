varying vec3 v_position;
varying vec2 v_texcoord0;
uniform sampler2D u_diffuseTexture;

void main() 
{ 
	gl_FragColor = texture2D(u_diffuseTexture, v_texcoord0); 
}