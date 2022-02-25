#version 330 core

#include "include/attributes.inc"

#include "include/matrices.inc"

uniform vec3 v3CameraPos;	// The camera's current position
uniform vec3 v3LightPos;	// The direction vector to the light source
uniform float fKmESun;		// Km * ESun

out vec3 v3Direction;
out vec4 v4MieColor;
out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord0;

void main(void)
{
  v_position = a_position;
  v_normal = a_normal;
  v_texcoord0 = a_texcoord0;

  // Get the ray from the camera to the vertex, and its length (which is the far point of the ray passing through the atmosphere)
  vec3 v3Pos = vec4(u_modelMatrix * vec4(a_position, 1.0)).xyz;
  
  // Finally, scale the Mie and Rayleigh colors and set up the varying variables for the pixel shader        
  v4MieColor = vec4(fKmESun, fKmESun, fKmESun, 1.0);
  
  v3Direction = v3CameraPos-v3Pos;
  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(a_position, 1.0);
}