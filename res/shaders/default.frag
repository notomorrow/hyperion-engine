//#version 330 core
#version 430 core

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.4

in vec4 v_position;
in vec4 v_positionCamspace;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

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

layout(std140) uniform TestBlock {
  vec3 my_color;  
};

void main()
{
  float roughness = clamp(u_roughness, 0.05, 0.99);
  float metallic = clamp(u_shininess, 0.05, 0.99);
  float ao = 0.0;

  vec3 lightDir = normalize(env_DirectionalLight.direction);
  vec3 n = normalize(v_normal.xyz);
  vec3 viewVector = normalize(u_camerapos-v_position.xyz);

  vec3 tangentViewPos = v_tbn * u_camerapos.xyz;//((u_viewMatrix * v_position)).xyz;
  vec3 tangentLightPos = v_tbn * lightDir;
  vec3 tangentFragPos = v_tbn * v_position.xyz;

  vec2 texCoords = v_texcoord0;

  if (HasParallaxMap == 1) {
    texCoords = ParallaxMapping(texCoords, normalize(tangentFragPos - tangentViewPos));//v_tbn * (u_viewMatrix * vec4(-v_position.xyz, 1.0)).xyz));
    if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0) {
      texCoords = v_texcoord0;
      //discard;
    }
  }

  vec4 diffuseTexture = vec4(1.0, 1.0, 1.0, 1.0);

  if (HasDiffuseMap == 1) {
    diffuseTexture = texture(DiffuseMap, texCoords);
  }
  
  if (HasAoMap == 1) {
    ao = 1.0-texture(AoMap, texCoords).r;
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
    vec4 normalsTexture = texture(NormalMap, texCoords) * 2.0 - 1.0;
    n = normalize(v_tbn * normalsTexture.rgb);
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

  vec3 CameraPosition = u_camerapos;
  vec3 position = v_position.xyz;
#include "include/inl/shadowing.inl.glsl"

  vec4 blurredSpecularCubemap = vec4(0.0);
  vec4 specularCubemap = vec4(0.0);
  vec4 gi = vec4(0.0);
  float roughnessMix = 1.0 - exp(-(roughness / 1.0 * log(100.0)));
#if 0
#if VCT_ENABLED
  //vec4 vctSpec = VCTSpecular(v_position.xyz, n.xyz, u_camerapos, u_roughness);
  //vec4 vctDiff = VCTDiffuse(v_position.xyz, n.xyz, u_camerapos, v_tangent, v_bitangent, roughness);
  //specularCubemap += vctSpec;
  //gi += vctDiff;
#endif // VCT_ENABLED

#if PROBE_ENABLED
  blurredSpecularCubemap = SampleEnvProbe(env_GlobalIrradianceCubemap, n, v_position.xyz, u_camerapos, 0.0);
  specularCubemap += SampleEnvProbe(env_GlobalCubemap, n, v_position.xyz, u_camerapos, v_tangent, v_bitangent);
  //specularCubemap += mix(SampleEnvProbe(env_GlobalCubemap, n, v_position.xyz, u_camerapos, v_tangent, v_bitangent), blurredSpecularCubemap, roughnessMix);
#if !SPHERICAL_HARMONICS_ENABLED
  //gi += EnvRemap(Irradiance(n));
#endif // !SPHERICAL_HARMONICS_ENABLED
#endif // PROBE_ENABLED

#if SPHERICAL_HARMONICS_ENABLED
  gi.rgb += SampleSphericalHarmonics(n);
#endif // SPHERICAL_HARMONICS_ENABLED
#endif

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
  reflectedLight += ibl * specularCubemap.rgb;

  vec3 diffRef = vec3((vec3(1.0) - F) * (1.0 / $PI) * NdotL);
  //diffRef += gi.rgb;
  diffuseLight += diffRef * (1.0 / $PI);
  diffuseLight *= metallicDiff;

  vec3 color = diffuseLight + reflectedLight;
  

  output0 = vec4(color, 1.0);
#endif

#if DEFERRED
  output0 = vec4(albedo.rgb, 1.0);
  output1 = vec4(normalize(n) * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(metallic, roughness, 0.0, 1.0);
  output4 = vec4(0.0, 0.0, 0.0, ao);
  output5 = vec4(normalize(v_tangent.xyz) * 0.5 + 0.5, 1.0);
  output6 = vec4(normalize(v_bitangent.xyz) * 0.5 + 0.5, 1.0);
#endif
}