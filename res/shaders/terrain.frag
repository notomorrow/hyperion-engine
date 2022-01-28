#version 330 core

#define $PI 3.141592654
#define $SLOPE_ANGLE 0.7
#define $LEVEL_THRESHOLD 10.0
#define $TRIPLANAR_BLENDING 1
#define $TERRAIN_LEVEL1 0

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

uniform sampler2D BaseTerrainColorMap;
uniform sampler2D BaseTerrainNormalMap;
uniform sampler2D BaseTerrainParallaxMap;
uniform sampler2D BaseTerrainAoMap;
uniform int HasBaseTerrainColorMap;
uniform int HasBaseTerrainNormalMap;
uniform int HasBaseTerrainParallaxMap;
uniform int HasBaseTerrainAoMap;
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

#include "include/matrices.inc"
#include "include/frag_output.inc"
#include "include/depth.inc"
#include "include/lighting.inc"
#include "include/parallax.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif


void main() 
{
  //float ang = 1.0 - v_normal.y;
  //float slopeBlend = 1.0 - clamp(ang / $SLOPE_ANGLE, 0.0, 1.0);

  // === parallax pre calculations ===
  float roughness = clamp(u_roughness, 0.05, 0.99);
  float metallic = clamp(u_shininess, 0.05, 0.99);

  // vec3 slopeBlend = abs(v_normal.xyz);
  // slopeBlend = (slopeBlend - vec3(0.2)) * 0.7;
  // slopeBlend = normalize(max(slopeBlend, 0.00001));      // Force weights to sum to 1.0 (very important!)
  // float b = (slopeBlend.x + slopeBlend.y + slopeBlend.z);
  // slopeBlend /= vec3(b, b, b);

  vec3 lightDir = normalize(env_DirectionalLight.direction);
  vec3 n = normalize(v_normal.xyz);
  vec3 viewVector = normalize(u_camerapos-v_position.xyz);

  vec3 tangentViewPos = v_tbn * viewVector;
  vec3 tangentLightPos = v_tbn * lightDir;
  vec3 tangentFragPos = v_tbn * v_position.xyz;

  vec2 flatTexCoord = v_texcoord0 * BaseTerrainScale;

  if (HasBaseTerrainParallaxMap == 1) {
    flatTexCoord = ParallaxMappingTexture(BaseTerrainParallaxMap, flatTexCoord, normalize(tangentViewPos - tangentFragPos));
  }

  // === flat terrain level ===
#if TRIPLANAR_BLENDING
  // in wNorm is the world-space normal of the fragment
  vec3 blending = abs(n);
  blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
  float b = (blending.x + blending.y + blending.z);
  blending /= vec3(b, b, b);

  vec4 flat_xaxis = texture(BaseTerrainColorMap, v_position.yz * BaseTerrainScale);
  vec4 flat_yaxis = texture(BaseTerrainColorMap, v_position.xz * BaseTerrainScale);
  vec4 flat_zaxis = texture(BaseTerrainColorMap, v_position.xy * BaseTerrainScale);
  // blend the results of the 3 planar projections.
  vec4 flatColor = flat_xaxis * blending.x + flat_yaxis * blending.y + flat_zaxis * blending.z;

  vec4 normal_xaxis = texture(BaseTerrainNormalMap, v_position.yz * BaseTerrainScale);
  vec4 normal_yaxis = texture(BaseTerrainNormalMap, v_position.xz * BaseTerrainScale);
  vec4 normal_zaxis = texture(BaseTerrainNormalMap, v_position.xy * BaseTerrainScale);

  vec4 flatNormals = normal_xaxis * blending.x + normal_yaxis * blending.y + normal_zaxis * blending.z;
#endif

#if !TRIPLANAR_BLENDING
  vec4 flatColor = texture(BaseTerrainColorMap, flatTexCoord);
  vec4 flatNormals = texture(BaseTerrainNormalMap, flatTexCoord);
#endif

  float flatAo = 1.0;
  
  //if (HasBaseTerrainAoMap == 1) {
  //  flatAo = texture(BaseTerrainAoMap, flatTexCoord).r;
  //}

  vec4 diffuseTexture = flatColor;
  vec4 normalsTexture = flatNormals;
  float ao = flatAo;

#if TERRAIN_LEVEL0
  // === level-based blending ====

  vec4 level1Color = texture(Level1ColorMap, v_texcoord0 * Level1Scale);
  vec4 level1Normals = texture(Level1NormalMap, v_texcoord0 * Level1Scale);
  float level1Ao = 1.0;

  float level1Amount = clamp((v_position.y) / Level1Height, 0.0, 1.0);

  diffuseTexture = mix(diffuseTexture, level1Color, level1Amount);
  normalsTexture = mix(normalsTexture, level1Normals, level1Amount);
  ao = mix(ao, level1Ao, level1Amount);
#endif

  // === slope-based blending ====
  // vec4 slopeColor1 = texture(SlopeColorMap, v_position.yz * SlopeScale);
  // vec4 slopeColor2 = texture(SlopeColorMap, v_position.xy * SlopeScale);
  // vec4 slopeNormal1 = texture(SlopeNormalMap, v_position.yz * SlopeScale);
  // vec4 slopeNormal2 = texture(SlopeNormalMap, v_position.xy * SlopeScale);

  // diffuseTexture = (slopeBlend.y * diffuseTexture + slopeBlend.x * slopeColor1 + slopeBlend.z * slopeColor2);
  // normalsTexture = (slopeBlend.y * normalsTexture + slopeBlend.x * slopeNormal1 + slopeBlend.z * slopeNormal2);
  
  
  // TODO: mixing for other tex's

  // if (ang >= $SLOPE_ANGLE) {
  //   diffuseTexture = slopeColor;
  //   normalsTexture = slopeNormal;
  // }

#if ROUGHNESS_MAPPING
  if (HasRoughnessMap == 1) {
    roughness = texture(RoughnessMap, flatTexCoord).r; // TODO: blending texcoords?
  }
#endif

#if METALNESS_MAPPING
  if (HasMetalnessMap == 1) {
    metallic = texture(MetalnessMap, flatTexCoord).r; // TODO: blending texcoords?
  }
#endif


#if NORMAL_MAPPING
  normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
  normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
  n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
#endif

  vec4 albedo = u_diffuseColor * diffuseTexture;

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
  int shadowSplit = getShadowMapSplit(distance(u_camerapos, v_position.xyz));

#if SHADOW_PCF
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            vec2 offset = poissonDisk[x * 4 + y] * $SHADOW_MAP_RADIUS;
            vec3 shadowCoord = getShadowCoord(shadowSplit, v_position.xyz + vec3(offset.x, offset.y, -offset.x));
            shadowness += getShadow(shadowSplit, shadowCoord, NdotL);
        }
    }

    shadowness /= 16.0;
#endif

#if !SHADOW_PCF
    vec3 shadowCoord = getShadowCoord(shadowSplit, v_position.xyz);
    shadowness = getShadow(shadowSplit, shadowCoord, NdotL);
#endif

  vec4 shadowColor = vec4(vec3(shadowness), 1.0);
  shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif


#if PROBE_ENABLED
    vec3 reflectionVector = EnvProbeVector(n, v_position.xyz, u_camerapos, u_modelMatrix);
#endif

#if !PROBE_ENABLED
    vec3 reflectionVector = ReflectionVector(n, v_position.xyz, u_camerapos);
#endif

  vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;
  vec3 blurredSpecularCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;

  float roughnessMix = clamp(1.0 - exp(-(roughness / 1.0 * log(100.0))), 0.0, 1.0);
  specularCubemap = mix(specularCubemap, blurredSpecularCubemap, roughnessMix);


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

  
  // vec3 irradiance = EnvRemap(Irradiance(n));
  // vec3 diffuse  = (albedo.rgb * irradiance) + (albedo.rgb * clamp(NdotL, 0.0, 1.0));
  // vec3 ambientColor = (diffuse * kD);

  // vec3 specTerm = (F * vec3(D * G * $PI) * brdf.x + brdf.y) * specularCubemap;
  

  vec3 color = diffuseLight + reflectedLight * shadowColor.rgb;
#endif

#if DEFERRED
  vec3 color = albedo.rgb;
#endif

  output0 = vec4(color, 1.0);
  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(metallic, roughness, 0.0, 1.0);
  output4 = vec4(0.0, 0.0, 0.0, ao);
}
