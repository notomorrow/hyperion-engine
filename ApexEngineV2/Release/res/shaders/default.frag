#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

uniform vec4 u_diffuseColor;
uniform sampler2D u_diffuseTexture;
uniform vec3 u_camerapos;

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

void main() 
{
  const float roughness = 0.3;
  const float shininess = 0.3;
  
  vec3 n = normalize(v_normal.xyz);
  vec3 v = normalize(u_camerapos - v_position.xyz);
  
  float ndotl = max(min(dot(n, env_DirectionalLight.direction), 1.0), 0.0);
  vec4 lighting = vec4(vec3(ndotl), 1.0);
  
  float specular = max(min(SpecularDirectional(n, v, env_DirectionalLight.direction, roughness), 1.0), 0.0);
    
  float fresnel;
  fresnel = max(1.0 - dot(n, v), 0.0);
  fresnel = pow(fresnel, 2.0);
  specular += fresnel;
  
  specular *= shininess;
  
#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.05;
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      vec2 offset = poissonDisk[x * 4 + y] * radius;
      vec3 shadowCoord = getShadowCoord(0, v_position.xyz + vec3(offset.x, offset.y, -offset.x));
      shadowness += getShadow(0, shadowCoord);
    }
  }
  shadowness /= 16.0;
  shadowness = mix(0.6, shadowness, ndotl);
#endif

#if !SHADOWS
  float shadowness = 1.0;
#endif
   
  vec4 ambient = vec4(vec3(0.3), 1.0);
  
#if DIFFUSE_MAP
  vec4 diffuseTexture = texture2D(u_diffuseTexture, v_texcoord0);
#endif
#if !DIFFUSE_MAP
  vec4 diffuseTexture = vec4(1.0);
#endif

  vec4 diffuse = clamp(lighting + ambient, vec4(0.0), vec4(1.0)) * diffuseTexture * u_diffuseColor;
  diffuse.rgb *= (1.0 - shininess);
  
  gl_FragColor = vec4((diffuse + vec4(specular)) * vec4(vec3(max(shadowness, 0.5)), 1.0));
}