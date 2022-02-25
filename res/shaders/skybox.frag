#version 330 core

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/frag_output.inc"
#include "include/lighting.inc"
#include "include/tonemap.inc"
#include "include/parallax.inc"

uniform samplerCube SkyboxMap;
uniform int HasSkyboxMap;

void main()
{
  vec3 color = texture(SkyboxMap, v_position.xyz).rgb;
  vec3 n = v_normal.xyz;
  
  
  // todo: standardize in engine
  float aperture = 16.0;
  float shutterSpeed = 1.0/125.0;
  float sensitivity = 100.0;
  float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity);
  float exposure = 1.0f / (1.2f * pow(2.0f, ev100));


  // Apply exposure.
  color *= exposure * $IBL_INTENSITY;
  color = tonemap(color);
  

  output0 = vec4(color, 1.0);
  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(0.0);
  output4 = vec4(0.0);
  output5 = vec4(0.0);
  output6 = vec4(0.0);
}
