#version 330

#include "include/matrices.inc"

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;

uniform vec3 v3CameraPos;	// The camera's current position
uniform vec3 v3LightPos;	// The direction vector to the light source
uniform float fOuterRadius;	// The outer (atmosphere) radius
uniform float fOuterRadius2;	// fOuterRadius^2
uniform float fInnerRadius;	// The inner (planetary) radius
uniform float fInnerRadius2;	// fInnerRadius^2
uniform float fKrESun;		// Kr * ESun
uniform float fKmESun;		// Km * ESun
uniform float fKr4PI;		// Kr * 4 * PI
uniform float fKm4PI;		// Km * 4 * PI

varying vec3 v3Direction;
varying vec4 v4RayleighColor;
varying vec4 v4MieColor;
varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_texcoord0;

void main(void)
{
  v_position = a_position;
  v_normal = a_normal;
  v_texcoord0 = a_texcoord0;
  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(a_position, 1.0);

  // Get the ray from the camera to the vertex, and its length (which is the far point of the ray passing through the atmosphere)
  vec3 v3Pos = vec4(u_modelMatrix * vec4(a_position, 1.0)).xyz;
  vec3 v3Start = v3CameraPos;
  float fHeight = length(v3Start);
  

  vec4 twilight = vec4(188.0/255.0, 66.0/255.0, 18.0/255.0, 1.0);
  vec4 blue = vec4(33.0/255.0, 74.0/255.0, 243.0/255.0, 1.0);
  
  float sunPosition = abs(v3LightPos.y);
  
  vec4 horizon = mix(twilight, blue*1.5, clamp(sunPosition, 0.0, 1.0));
  vec4 zenith = blue;
  
  float sun = 1.0 + 6.50 * exp(-fHeight * fHeight / fOuterRadius * fOuterRadius);
  
  // Finally, scale the Mie and Rayleigh colors and set up the varying variables for the pixel shader        
  v4MieColor = vec4(fKmESun, fKmESun, fKmESun, 1.0);
  
  v4RayleighColor = vec4(mix(zenith.rgb, horizon.rgb, pow(1.0 - abs(a_position.y*1.5), 2.0)), 1.0);
  v4RayleighColor *= clamp(v3LightPos.y * 0.5 + 0.5, 0.2, 1.0);
  
  v3Direction = v3CameraPos - v3Pos;
}