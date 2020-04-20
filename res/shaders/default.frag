#version 330 core

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/depth.inc"
#include "include/frag_output.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

#include "include/parallax.inc"

void main()
{
  float metallic = u_shininess;

  vec3 lightDir = normalize(env_DirectionalLight.direction);

  vec3 n = normalize(v_normal.xyz);
  vec3 v = normalize(u_camerapos - v_position.xyz);

  vec3 tangentViewPos = v_tbn * v;
  vec3 tangentLightPos = v_tbn * lightDir;
  vec3 tangentFragPos = v_tbn * v_position.xyz;

  vec2 texCoords = v_texcoord0;

  if (u_hasParallaxMap == 1) {
    texCoords = ParallaxMapping(v_texcoord0, normalize(tangentViewPos - tangentFragPos));
  }

  vec4 diffuseTexture = texture(u_diffuseMap, texCoords);


#if NORMAL_MAPPING
  if (u_hasNormalMap == 1) {
    vec4 normalsTexture = texture(u_normalMap, texCoords);

    normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
    normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
    n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
  }
#endif


#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.1;
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
  //shadowness *= 1.0 - dot(n, lightDir);
  vec4 shadowColor = vec4(vec3(max(shadowness, 0.5)), 1.0);
  shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, 0.0, u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif


  float fresnel;
  fresnel = max(1.0 - dot(n, v), 0.0);
  fresnel = pow(fresnel, 2.0);


  float ndotl = max(min(dot(n, lightDir), 1.0), 0.0);
  vec4 lighting = vec4(0.0, 0.0, 0.0, 1.0);//vec4(vec3(1.0 - fresnel) * vec3(1.0 / PI) * vec3(ndotl), 1.0);

  vec3 albedo = u_diffuseColor.rgb;

  if (u_hasDiffuseMap == 1) {
    albedo *= diffuseTexture.rgb;
  }

  // specular
  vec3 Lo = vec3(0.0);

  float NdotL = dot(n, lightDir);
	float NdotV = dot(n, v);
	vec3 H = normalize(lightDir + v);
	float NdotH = dot(n, H);
	float LdotH = dot(lightDir, H);

  vec3 F0 = vec3(0.04);
  F0 = mix(albedo, F0, metallic);


  vec2 brdfSamplePoint = clamp(vec2(NdotV, u_roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  // retrieve a scale and bias to F0. See [1], Figure 3
  vec2 brdf = texture(u_brdfMap, brdfSamplePoint).rg;


  Lo += ComputeDirectionalLight(env_DirectionalLight, n, v,  v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  float mipLevel = u_roughness * 11.0;

  vec3 diffuseCubemap = texture(env_GlobalCubemap, n, mipLevel).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, ReflectionVector(n, v_position.xyz, u_camerapos.xyz), mipLevel).rgb;


  vec3 ambientSpecF = SchlickFresnelRoughness(F0, LdotH);
  vec3 ambientDiffuse = vec3(1.0) - ambientSpecF;
  ambientDiffuse *= 1.0 - metallic;
  vec3 ambient = ((diffuseCubemap * albedo) * ambientDiffuse) + (specularCubemap * (albedo * brdf.x + brdf.y));//vec3(0.1) * (specular);

  if (u_hasAoMap == 1) {
    ambient *= texture(u_aoMap, texCoords).r;
  }

  for (int i = 0; i < env_NumPointLights; i++) {
    //Lo += ComputePointLight(env_PointLights[i], n, normalize(u_camerapos.xyz - v_position.xyz), v_position.xyz, albedo, shadowness, u_roughness, u_shininess);
  }


  vec3 color = (ambient + Lo) * shadowColor.rgb;


  output0 = vec4(color, 1.0);//vec4(color, 1.0);
  output1 = vec4(n.xyz, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  outputDepth = packDepth(gl_FragCoord.z);
}
