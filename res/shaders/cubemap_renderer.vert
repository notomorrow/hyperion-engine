#version 330 core

#include "include/attributes.inc"

out vec4 v_position;
out vec4 v_positionCamspace;
out vec4 v_normal;
out vec2 v_texcoord0;
out vec3 v_tangent;
out vec3 v_bitangent;
out mat3 v_tbn;

#include "include/matrices.inc"
#include "include/lighting.inc"

#define $CUBEMAP_LIGHTING_AMBIENT 0.2

out VSOutput
{
  vec3 position;
	vec3 normal;
	vec2 texcoord0;
#if PROBE_RENDER_SHADING
  vec4 lighting;
#endif
} vs_out;

void main() {
  v_position = u_modelMatrix * vec4(a_position, 1.0);
  vs_out.normal = a_normal.xyz;
  vs_out.texcoord0 = a_texcoord0;
  vs_out.position = v_position.xyz;

#if PROBE_RENDER_SHADING
  // vertex shading
  float NdotL = clamp(dot(a_normal.xyz, env_DirectionalLight.direction), $CUBEMAP_LIGHTING_AMBIENT, 1.0);
  vs_out.lighting = vec4(NdotL, NdotL, NdotL, 1.0) * env_DirectionalLight.color;
#endif

  gl_Position = v_position;
}
