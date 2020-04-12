#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

uniform vec4 u_diffuseColor;
uniform sampler2D u_diffuseMap;
uniform vec3 u_camerapos;

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

void main() 
{
  float metallic = u_shininess;
  vec4 diffuseTexture = texture2D(u_diffuseMap, v_texcoord0);

  vec3 n = normalize(v_normal.xyz);
  vec3 v = normalize(u_camerapos - v_position.xyz);
  
  
#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.05;
  int shadowSplit = getShadowMapSplit(u_camerapos, v_position.xyz);
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      vec2 offset = poissonDisk[x * 4 + y] * radius;
      vec3 shadowCoord = getShadowCoord(shadowSplit, v_position.xyz + vec3(offset.x, offset.y, -offset.x));
      shadowness += getShadow(shadowSplit, shadowCoord);
    }
  }
  shadowness /= 16.0;
  //shadowness -= length(pl_specular);
  //shadowness = mix(0.6, shadowness, ndotl);
#endif

#if !SHADOWS
  float shadowness = 1.0;
#endif
  
  
  float fresnel;
  fresnel = max(1.0 - dot(n, v), 0.0);
  fresnel = pow(fresnel, 2.0);
 
  
  vec3 lightDir = normalize(env_DirectionalLight.direction);
  float ndotl = max(min(dot(n, lightDir), 1.0), 0.0);
  vec4 lighting = vec4(0.0, 0.0, 0.0, 1.0);//vec4(vec3(1.0 - fresnel) * vec3(1.0 / PI) * vec3(ndotl), 1.0);
  
  vec3 albedo = diffuseTexture.rgb;
  vec3 diffuse = albedo;

  // specular
  vec3 Lo = vec3(0.0);
  
  float NdotL = dot(n, lightDir);
	float NdotV = dot(n, v);
	vec3 H = normalize(lightDir + v);
	float NdotH = dot(n, H);
	float LdotH = dot(lightDir, H);
  
  vec3 F0 = vec3(0.04);
  F0 = mix(albedo, F0, metallic);

  Lo += ComputeDirectionalLight(env_DirectionalLight, n, v,  v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  float mipLevel = u_roughness * 11.0;
	vec3 cubeMap = texture(env_GlobalCubemap, ReflectionVector(n, normalize(u_camerapos.xyz - v_position.xyz), u_camerapos.xyz), mipLevel).rgb;
  vec3 ambientSpecF = SchlickFresnelRoughness(F0, LdotH);
  vec3 ambientDiffuse = vec3(1.0) - ambientSpecF;
  ambientDiffuse *= 1.0 - metallic;
  //vec3 specular = cubeMap * fresnel;
  vec3 ambient = vec3(1.0 - metallic) * (cubeMap * ambientSpecF);//vec3(0.1) * (specular);
  
  for (int i = 0; i < env_NumPointLights; i++) {
    Lo += ComputePointLight(env_PointLights[i], n, normalize(u_camerapos.xyz - v_position.xyz), v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  }
  
  
  vec3 color = ambient + Lo;
  //specular += pl_specular;
  //specular *= u_shininess;


  //diffuse *= clamp(shadowness, 0.0, 0.5);
  //diffuse.rgb *= (1.0 - u_shininess);
  
  gl_FragData[0] = vec4(color, 1.0);
  gl_FragData[1] = vec4(n.xyz, 1.0);
  gl_FragData[2] = vec4(v_position.xyz, 1.0);
}