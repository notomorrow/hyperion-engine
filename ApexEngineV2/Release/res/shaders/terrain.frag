#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;
varying vec3 v_tangent;
varying vec3 v_bitangent;

uniform vec3 u_camerapos;

uniform sampler2D terrainTexture0;
uniform sampler2D terrainTexture0Normal;
uniform int terrainTexture0HasNormal;
uniform float terrainTexture0Scale;

uniform sampler2D slopeTexture;
uniform sampler2D slopeTextureNormal;
uniform float slopeTextureScale;

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

void main() 
{
  const float roughness = 0.6;
  const float shininess = 0.1;

  vec3 dir = env_DirectionalLight.direction;
  dir.y = abs(dir.y);
  
  vec3 up = vec3(0.0, 1.0, 0.0);
  float ang = max(abs(v_normal.x), 0.0);
  float tex0Strength = 1.0 - clamp(ang / 0.5, 0.0, 1.0);

  vec4 flatColor = texture2D(terrainTexture0, v_texcoord0 * terrainTexture0Scale);
  vec4 slopeColor = texture2D(slopeTexture, v_texcoord0 * slopeTextureScale);
  vec4 diffuseTexture = mix(slopeColor, flatColor, tex0Strength);

  if (ang >= 0.5) {
    diffuseTexture = slopeColor;
  }
  
  vec3 n = normalize(v_normal.xyz);
  vec3 v = normalize(u_camerapos - v_position.xyz);

#if NORMAL_MAP
  vec4 flatNormals = texture2D(terrainTexture0Normal, v_texcoord0 * terrainTexture0Scale);
  vec4 slopeNormals = texture2D(slopeTextureNormal, v_texcoord0 * slopeTextureScale);
  vec4 normalsTexture = mix(slopeNormals, flatNormals, tex0Strength);

  normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
  normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
  n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
#endif
  
  float ndotl = max(min(dot(n, dir), 1.0), 0.0);
  
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

  vec4 shadowColor = vec4(vec3(max(shadowness, 0.5)), 1.0);
  shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, 0.0, 50.0);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif

  vec4 lighting = vec4(vec3(ndotl), 1.0) * env_DirectionalLight.color;
  
  vec4 specular = vec4(max(min(SpecularDirectional(n, v, dir, roughness), 1.0), 0.0));

  float fresnel;
  fresnel = max(1.0 - dot(n, v), 0.0);
  fresnel = pow(fresnel, 2.0);
  specular += vec4(fresnel);
  
  specular *= env_DirectionalLight.color;
  specular *= shininess;
  //specular *= ndotl;
  specular *= shadowness;
   
  vec4 ambient = vec4(vec3(0.3), 1.0) * diffuseTexture * env_DirectionalLight.color;
  ambient.rgb *= (1.0 - shininess);
  ambient *= shadowColor;
  
  vec3 diffuse = clamp(lighting.rgb, vec3(0.0), vec3(1.0)) * diffuseTexture.rgb;
  diffuse.rgb *= (1.0 - shininess);
  diffuse *= shadowColor.rgb;
  
  vec4 fogColor = env_DirectionalLight.color * env_DirectionalLight.color * env_DirectionalLight.color;

  gl_FragColor = CalculateFogExp(vec4((vec4(diffuse, 1.0) + ambient + specular)), fogColor, v_position.xyz, u_camerapos, 180.0, 200.0);
}