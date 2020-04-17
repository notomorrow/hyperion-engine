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
  vec3 lightDir = env_DirectionalLight.direction;
  //lightDir.y = abs(lightDir.y);
  
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
  
  //mat3 TBN = mat3(v_tangent, v_bitangent, n)

#if NORMAL_MAPPING
  vec4 flatNormals = texture2D(terrainTexture0Normal, v_texcoord0 * terrainTexture0Scale);
  vec4 slopeNormals = texture2D(slopeTextureNormal, v_texcoord0 * slopeTextureScale);
  vec4 normalsTexture = mix(slopeNormals, flatNormals, tex0Strength);

  normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
  normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
  //normalsTexture = 2.0 * normalsTexture - vec3(1.0);
  n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
#endif
  
  float ndotl = max(min(dot(n, lightDir), 1.0), 0.0);
  
#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.1;
  int shadowSplit = getShadowMapSplit(u_camerapos, v_position.xyz);
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      vec2 offset = poissonDisk[x * 4 + y] * radius;
      vec3 shadowCoord = getShadowCoord(3, v_position.xyz + vec3(offset.x, offset.y, -offset.x));
      shadowness += getShadow(3, shadowCoord);
    }
  }
  shadowness /= 16.0;

  vec4 shadowColor = vec4(vec3(max(shadowness, 0.5)), 1.0);
  //shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, 0.0, u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif

  vec4 lighting = vec4(vec3(ndotl), 1.0) * env_DirectionalLight.color;
  
  vec3 albedo = diffuseTexture.rgb;
  vec3 diffuse = albedo;

  // specular
  vec3 Lo = vec3(0.0);
  float metallic = u_shininess;
 
  float NdotL = dot(n, lightDir);
	float NdotV = dot(n, v);
	vec3 H = normalize(lightDir + v);
	float NdotH = dot(n, H);
	float LdotH = dot(lightDir, H);

  vec3 F0 = vec3(0.04);
  F0 = mix(albedo, F0, metallic);


  vec2 brdfSamplePoint = clamp(vec2(NdotV, u_roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  // retrieve a scale and bias to F0. See [1], Figure 3
  vec2 brdf = texture2D(u_brdfMap, brdfSamplePoint).rg;

  Lo += ComputeDirectionalLight(env_DirectionalLight, n, normalize(u_camerapos.xyz - v_position.xyz), v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  
  for (int i = 0; i < env_NumPointLights; i++) {
    Lo += ComputePointLight(env_PointLights[i], n, normalize(u_camerapos.xyz - v_position.xyz), v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  }
  
  float mipLevel = u_roughness * 11.0;

  vec3 diffuseCubemap = texture(env_GlobalCubemap, n, mipLevel).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, ReflectionVector(n, v_position.xyz, u_camerapos.xyz), mipLevel).rgb;

  vec3 ambientSpecF = SchlickFresnelRoughness(F0, LdotH);
  vec3 ambientDiffuse = vec3(1.0) - ambientSpecF;
  ambientDiffuse *= 1.0 - metallic;
  vec3 ambient = ((diffuseCubemap * albedo) * ambientDiffuse) + (specularCubemap * (albedo * brdf.x + brdf.y));
  
  vec4 specular = vec4(max(min(SpecularDirectional(n, v, lightDir, u_roughness), 1.0), 0.0));

  
  specular *= env_DirectionalLight.color;
  specular *= u_shininess;
  specular *= shadowness;
   
  vec4 ambient_color = vec4(vec3(0.01), 1.0) * env_DirectionalLight.color;
  ambient_color.rgb *= (1.0 - u_shininess);
  ambient_color *= shadowColor;
  
  diffuse = clamp(lighting.rgb, vec3(0.0), vec3(1.0)) * diffuseTexture.rgb;
  diffuse *= (1.0 - u_shininess);
  diffuse *= shadowColor.rgb;
  
  vec4 fogColor = env_DirectionalLight.color * env_DirectionalLight.color * env_DirectionalLight.color;
  fogColor.a = 1.0;
  

  //vec3 color = (ambient + Lo) * shadowColor.rgb;
  vec3 color = (ambient_color.rgb + diffuse.rgb + specular.rgb);


  gl_FragData[1] = vec4(n.xyz, 1.0);
  gl_FragData[2] = vec4(v_position.xyz, 1.0);
 
  gl_FragData[0] = CalculateFogExp(vec4(color.rgb, 1.0), fogColor, v_position.xyz, u_camerapos, 320.0, 600.0);
}
