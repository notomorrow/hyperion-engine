//#version 330 core
#version 430 core

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.08

in vec4 v_position;
in vec4 v_positionCamspace;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;
in vec4 v_voxelPosition;

uniform vec3 u_camerapos;

#include "include/matrices.inc"
#include "include/frag_output.inc"
#include "include/depth.inc"
#include "include/lighting.inc"
#include "include/parallax.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif

vec4 encodeNormal(vec3 n)
{
    float p = sqrt(n.z*8+8);
    return vec4(n.xy/p + 0.5,0,0);
}

void main()
{
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

  vec4 diffuseTexture = vec4(1.0, 1.0, 1.0, 1.0);

  if (HasDiffuseMap == 1) {
    diffuseTexture = texture(DiffuseMap, texCoords);
  }

  vec4 albedo = u_diffuseColor * diffuseTexture;

  if (albedo.a < $ALPHA_DISCARD) {
    discard;
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
  if (HasNormalMap == 1) {
    vec4 normalsTexture = texture(NormalMap, texCoords);
    normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
    normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
    n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
  }
#endif

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


  vec3 blurredSpecularCubemap = vec3(0.0);
  vec3 specularCubemap = vec3(0.0);
  vec3 diffuseCubemap = vec3(0.0);

#if VCT_ENABLED
  //testing
  vec3 voxel = v_voxelPosition.xyz;
  vec4 vctSpec = VCTSpecular(voxel, v_position.xyz, n.xyz, u_camerapos);
  vec4 vctDiff = VCTDiffuse(voxel, v_position.xyz, n.xyz, u_camerapos, v_tangent, v_bitangent);
  specularCubemap = vctSpec.rgb;
  diffuseCubemap = vctDiff.rgb;
#endif // VCT_ENABLED

#if !VCT_ENABLED
  diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
#endif // !VCT_ENABLED

#if PROBE_ENABLED
  blurredSpecularCubemap = SampleEnvProbe(env_GlobalIrradianceCubemap, n, v_position.xyz, u_camerapos).rgb;
#if !VCT_ENABLED
  specularCubemap = SampleEnvProbe(env_GlobalCubemap, n, v_position.xyz, u_camerapos).rgb;
#endif // !VCT_ENABLED
#endif // PROBE_ENABLED

#if !PROBE_ENABLED
  vec3 reflectionVector = ReflectionVector(n, v_position.xyz, u_camerapos);
  blurredSpecularCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;
#if !VCT_ENABLED
  specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;
#endif // !VCT_ENABLED
#endif // !PROBE_ENABLED

  float roughnessMix = 1.0 - exp(-(roughness / 1.0 * log(100.0)));
  //specularCubemap = mix(specularCubemap, blurredSpecularCubemap, roughness);


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
#if VCT_ENABLED
  diffRef += diffuseCubemap;// * (1.0 / $PI); // use diffuse cubemap as GI
#endif
  diffuseLight += diffRef;

//#if !VCT_ENABLED
  diffuseLight += EnvRemap(Irradiance(n)) * (1.0 / $PI);
//#endif
  diffuseLight *= metallicDiff;

  vec3 color = diffuseLight + reflectedLight * shadowColor.rgb;
  

  output0 = vec4(color, 1.0);
#endif

#if DEFERRED
 output0 = albedo;
#endif

  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(metallic, roughness, 0.0, 1.0);
  output4 = vec4(0.0, 0.0, 0.0, 0.0); // TODO: alpha should be aomap
  output5 = vec4(v_tangent * 0.5 + 0.5, 1.0);
  output6 = vec4(v_bitangent * 0.5 + 0.5, 1.0);
}