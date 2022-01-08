#version 330 core

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/frag_output.inc"

#include "include/parallax.inc"

uniform samplerCube SkyboxMap;
uniform int HasSkyboxMap;

void main()
{
  
  vec3 color = texture(SkyboxMap, v_position.xyz).rgb;


  output0 = vec4(color, 1.0);
}
