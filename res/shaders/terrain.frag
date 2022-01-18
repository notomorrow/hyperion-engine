#version 330 core

#define $PI 3.141592654
#define $SLOPE_ANGLE 0.7
#define $LEVEL_THRESHOLD 10.0

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

uniform sampler2D BaseTerrainColorMap;
uniform sampler2D BaseTerrainNormalMap;
uniform int HasBaseTerrainColorMap;
uniform int HasBaseTerrainNormalMap;
uniform float BaseTerrainScale;

uniform sampler2D Level1ColorMap;
uniform sampler2D Level1NormalMap;
uniform int HasLevel1ColorMap;
uniform int HasLevel1NormalMap;
uniform float Level1Scale;
uniform float Level1Height;

uniform sampler2D SlopeColorMap;
uniform sampler2D SlopeNormalMap;
uniform float SlopeScale;

#include "include/frag_output.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

#include "include/parallax.inc"

void main() 
{
  //float ang = 1.0 - v_normal.y;
  //float slopeBlend = 1.0 - clamp(ang / $SLOPE_ANGLE, 0.0, 1.0);

  vec3 slopeBlend = abs(v_normal.xyz);
  slopeBlend = (slopeBlend - vec3(0.2)) * 0.7;
  slopeBlend = normalize(max(slopeBlend, 0.00001));      // Force weights to sum to 1.0 (very important!)
  float b = (slopeBlend.x + slopeBlend.y + slopeBlend.z);
  slopeBlend /= vec3(b, b, b);

  vec4 flatColor = texture(BaseTerrainColorMap, v_texcoord0 * BaseTerrainScale);
  vec4 flatNormal = texture(BaseTerrainNormalMap, v_texcoord0 * BaseTerrainScale);

  vec4 level1Color = texture(Level1ColorMap, v_texcoord0 * Level1Scale);
  vec4 level1Normal = texture(Level1NormalMap, v_texcoord0 * Level1Scale);

  vec4 slopeColor1 = texture(SlopeColorMap, v_position.yz * SlopeScale);
  vec4 slopeColor2 = texture(SlopeColorMap, v_position.xy * SlopeScale);
  vec4 slopeNormal1 = texture(SlopeNormalMap, v_position.yz * SlopeScale);
  vec4 slopeNormal2 = texture(SlopeNormalMap, v_position.xy * SlopeScale);

  float level1Amount = clamp((v_position.y) / Level1Height, 0.0, 1.0);

  vec4 diffuseTexture = mix(flatColor, level1Color, level1Amount);
  vec4 normalsTexture = mix(flatNormal, level1Normal, level1Amount);

  diffuseTexture = (slopeBlend.y * diffuseTexture + slopeBlend.x * slopeColor1 + slopeBlend.z * slopeColor2);
  normalsTexture = (slopeBlend.y * normalsTexture + slopeBlend.x * slopeNormal1 + slopeBlend.z * slopeNormal2);

  /*if (ang >= $SLOPE_ANGLE) {
    diffuseTexture = slopeColor;
    normalsTexture = slopeNormal;
  }*/
  
  float roughness = clamp(u_roughness, 0.05, 0.99);
  float metallic = clamp(u_shininess, 0.05, 0.99);

  vec3 lightDir = normalize(env_DirectionalLight.direction);
  vec3 n = normalize(v_normal.xyz);
  vec3 viewVector = normalize(u_camerapos-v_position.xyz);

  vec3 tangentViewPos = v_tbn * viewVector;
  vec3 tangentLightPos = v_tbn * lightDir;
  vec3 tangentFragPos = v_tbn * v_position.xyz;

  vec2 texCoords = v_texcoord0;

  if (HasParallaxMap == 1) {
    texCoords = ParallaxMapping(texCoords, normalize(tangentViewPos - tangentFragPos));
  }

#if ROUGHNESS_MAPPING
  if (HasRoughnessMap == 1) {
    roughness = texture(RoughnessMap, texCoords).r;
  }
#endif

#if METALNESS_MAPPING
  if (HasMetalnessMap == 1) {
    metallic = texture(MetalnessMap, texCoords).r;
  }
#endif


#if NORMAL_MAPPING
  normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
  normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
  n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
#endif

  vec4 albedo = u_diffuseColor * diffuseTexture;

  if (albedo.a < 0.1) {
    discard;
  }

#if !DEFERRED

  float NdotL = max(0.0, dot(n, lightDir));
  float NdotV = max(0.001, dot(n, viewVector));
  vec3 H = normalize(lightDir + viewVector);
  float NdotH = max(0.001, dot(n, H));
  float LdotH = max(0.001, dot(lightDir, H));
  float VdotH = max(0.001, dot(viewVector, H));
  float HdotV = max(0.001, dot(H, viewVector));


#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.075;
  int shadowSplit = getShadowMapSplit(u_camerapos, v_position.xyz);

  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      vec2 offset = poissonDisk[x * 4 + y] * radius;
      vec3 shadowCoord = getShadowCoord(shadowSplit, v_position.xyz + vec3(offset.x, offset.y, -offset.x));
      shadowness += getShadow(shadowSplit, shadowCoord);
    }
  }
  shadowness /= 16.0;
  shadowness *= 1.0 - NdotL;
  vec4 shadowColor = vec4(vec3(shadowness), 1.0);
  shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif

  int mipLevel = 0;//int(floor(roughness * 5));

  vec3 reflectionVector = ReflectionVector(n, v_position.xyz, u_camerapos.xyz);
  vec3 irradianceCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;

  vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;


  float roughnessMix = 1.0 - exp(-(roughness / 1.0 * log(100.0)));
  specularCubemap = mix(specularCubemap, irradianceCubemap, roughnessMix);


  vec3 F0 = vec3(0.04);
  F0 = mix(vec3(1.0), F0, metallic);

  vec2 AB = BRDFMap(NdotV, metallic);

  vec3 metallicSpec = mix(vec3(0.04), albedo.rgb, metallic);
  vec3 metallicDiff = mix(albedo.rgb, vec3(0.0), metallic);

  vec3 F = FresnelTerm(metallicSpec, VdotH) * clamp(NdotL, 0.0, 1.0);
  float D = clamp(DistributionGGX(n, H, roughness), 0.0, 1.0);
  float G = clamp(SmithGGXSchlickVisibility(clamp(NdotL, 0.0, 1.0), clamp(NdotV, 0.0, 1.0), roughness), 0.0, 1.0);
  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= vec3(1.0 - metallic);

  vec3 reflectedLight = vec3(0.0, 0.0, 0.0);
  vec3 diffuseLight = vec3(0.0, 0.0, 0.0);

  float rim = mix(1.0 - roughnessMix * 1.0 * 0.9, 1.0, NdotV);
  vec3 specRef = vec3((1.0 / max(rim, 0.0001)) * F * G * D) * NdotL;
  reflectedLight += specRef;

  vec3 ibl = min(vec3(0.99), FresnelTerm(metallicSpec, NdotV) * AB.x + AB.y);
  reflectedLight += ibl * specularCubemap;


  vec3 diffRef = vec3((vec3(1.0) - F) * (1.0 / $PI) * NdotL);
  diffuseLight += diffRef;
  diffuseLight += EnvRemap(Irradiance(n)) * (1.0 / $PI);
  diffuseLight *= metallicDiff;

  
  /*vec3 irradiance = EnvRemap(Irradiance(n));
  vec3 diffuse  = (albedo.rgb * irradiance) + (albedo.rgb * clamp(NdotL, 0.0, 1.0));
  vec3 ambientColor = (diffuse * kD);

  vec3 specTerm = (F * vec3(D * G * $PI) * brdf.x + brdf.y) * specularCubemap;

  if (HasAoMap == 1) {
    float ao = texture(AoMap, texCoords).r;
    //diffuseCubemap *= texture(AoMap, texCoords).r;
    //specularCubemap *= texture(AoMap, texCoords).r;
    ambientColor *= ao;
    specTerm *= clamp(pow(NdotV + ao, roughness * roughness) - 1.0 + ao, 0.0, 1.0);
  }*/

  vec3 color = diffuseLight + reflectedLight * shadowColor.rgb;
#endif

#if DEFERRED
  vec3 color = albedo.rgb;
#endif

  output0 = vec4(color, albedo.a);
  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(v_texcoord0.x, v_texcoord0.y, metallic, roughness);
}
