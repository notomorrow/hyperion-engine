#version 330

#include "include/matrices.inc"

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texcoord0;

uniform vec3 v3CameraPos;	// The camera's current position
uniform vec3 v3LightPos;	// The direction vector to the light source
uniform vec3 v3InvWavelength;	// 1 / pow(wavelength, 4) for the red, green, and blue channels
uniform float fCameraHeight;	// The camera's current height
uniform float fCameraHeight2;	// fCameraHeight^2
uniform float fOuterRadius;	// The outer (atmosphere) radius
uniform float fOuterRadius2;	// fOuterRadius^2
uniform float fInnerRadius;	// The inner (planetary) radius
uniform float fInnerRadius2;	// fInnerRadius^2
uniform float fKrESun;		// Kr * ESun
uniform float fKmESun;		// Km * ESun
uniform float fKr4PI;		// Kr * 4 * PI
uniform float fKm4PI;		// Km * 4 * PI
uniform float fScale;		// 1 / (fOuterRadius - fInnerRadius)
uniform float fScaleDepth;	// The scale depth (i.e. the altitude at which the atmosphere's average density is found)
uniform float fScaleOverScaleDepth; // fScale / fScaleDepth
uniform int nSamples;
uniform float fSamples;

varying vec3 v3Direction;
varying vec4 v4RayleighColor;
varying vec4 v4MieColor;
varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_texcoord0;

float scale(float fCos)
{
  float x = 1.0 - fCos;
  return fScaleDepth * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));
}

void main(void)
{
  v_position = a_position;
  v_normal = a_normal;
  v_texcoord0 = a_texcoord0;
  gl_Position = u_projMatrix * u_viewMatrix * u_modelMatrix * vec4(a_position, 1.0);

        // Get the ray from the camera to the vertex, and its length (which is the far point of the ray passing through the atmosphere)
  vec3 v3Pos = vec4(u_modelMatrix * vec4(a_position, 1.0)).xyz;
  vec3 v3Ray = v3Pos - v3CameraPos;
  float fFar = length(v3Ray);
  v3Ray /= fFar;

	// Calculate the ray's starting position, then calculate its scattering offset
  vec3 v3Start = v3CameraPos;
  float fHeight = length(v3Start);
  float fDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fCameraHeight));
  float fStartAngle = dot(v3Ray, v3Start) / fHeight;
  float fStartOffset = fDepth * scale(fStartAngle);

  // Initialize the scattering loop variables
  float fSampleLength = fFar / fSamples;
  float fScaledLength = fSampleLength * fScale;
  vec3 v3SampleRay = v3Ray * fSampleLength;
  vec3 v3SamplePoint = v3Start + v3SampleRay * 0.5;

  vec4 twilight = vec4(188.0/255.0, 66.0/255.0, 18.0/255.0, 1.0);
  vec4 blue = vec4(33.0/255.0, 74.0/255.0, 243.0/255.0, 1.0);
  
  float sunPosition = abs(v3LightPos.y);
  
  vec4 horizon = mix(twilight, blue*1.5, clamp(sunPosition, 0.0, 1.0));
  vec4 zenith = blue;//mix(twilight*1.5, blue, clamp(sunPosition, 0.0, 1.0));
  
	
  // The "sun" factor brightens the atmosphere trying to cover the starfield
    //float sun = 0.90 + 2.0 * exp(-pow(fHeight,5.0)/pow(fOuterRadius,5.0));
  float sun = 1.0 + 6.50 * exp(-fHeight * fHeight / fOuterRadius * fOuterRadius);
  
  // Finally, scale the Mie and Rayleigh colors and set up the varying variables for the pixel shader        
  v4MieColor = vec4(fKmESun, fKmESun, fKmESun, 1.0);
  //v4RayleighColor = vec4((v3InvWavelength * fKrESun * sun), 1.0);
    
  //v4RayleighColor = vec4(mix(twilight.rgb, colorDay.rgb, a_position.y * 0.5 + 0.5), 1.0);//*sun;
  //v4RayleighColor *= clamp(v3LightPos.y, 0.2, 1.0);
  
  v4RayleighColor = vec4(mix(zenith.rgb, horizon.rgb, pow(1.0 - abs(a_position.y*1.5), 2.0)), 1.0);
  v4RayleighColor *= clamp(v3LightPos.y * 0.5 + 0.5, 0.2, 1.0);
  
  v3Direction = v3CameraPos - v3Pos;
}