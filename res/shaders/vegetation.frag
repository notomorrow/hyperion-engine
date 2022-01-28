#version 330 core

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.08
#define $VEGETATION_FADE_START 50.0
#define $VEGETATION_FADE_END 80.0

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
#include "include/lighting.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif



void main()
{
  vec3 lightDir = normalize(env_DirectionalLight.direction);
  vec3 n = normalize(v_normal.xyz);
  vec3 viewVector = normalize(u_camerapos-v_position.xyz);

  vec3 tangentViewPos = v_tbn * viewVector;
  vec3 tangentLightPos = v_tbn * lightDir;
  vec3 tangentFragPos = v_tbn * v_position.xyz;

  vec2 texCoords = v_texcoord0;

  vec4 diffuseTexture = vec4(1.0, 1.0, 1.0, 1.0);

  if (HasDiffuseMap == 1) {
    diffuseTexture = texture(DiffuseMap, texCoords);
  }

  vec4 albedo = u_diffuseColor * diffuseTexture;

  if (albedo.a < $ALPHA_DISCARD) {
    discard;
  }

#if NORMAL_MAPPING
  if (HasNormalMap == 1) {
    vec4 normalsTexture = texture(NormalMap, texCoords);
    normalsTexture.xy = (2.0 * (vec2(1.0) - normalsTexture.rg) - 1.0);
    normalsTexture.z = sqrt(1.0 - dot(normalsTexture.xy, normalsTexture.xy));
    n = normalize((v_tangent * normalsTexture.x) + (v_bitangent * normalsTexture.y) + (n * normalsTexture.z));
  }
#endif

  float NdotL = max(0.0, dot(n, lightDir));
  vec3 H = normalize(lightDir + viewVector);
  float VdotH = max(0.001, dot(viewVector, H));

#if !DEFERRED

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

#endif

#if VEGETATION_LIGHTING
  vec3 F = FresnelTerm(albedo.rgb, VdotH) * clamp(NdotL, 0.0, 1.0);
  vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
  vec3 diffuseLight = vec3(0.0, 0.0, 0.0);
  vec3 diffRef = vec3((vec3(1.0) - F) * (1.0 / $PI) * NdotL);
  diffuseLight += diffRef;
  diffuseLight += EnvRemap(Irradiance(n)) * (1.0 / $PI);
  diffuseLight *= albedo.rgb;
  
  vec3 color = diffuseLight; // ambient light?
#endif

#if !VEGETATION_LIGHTING
  vec3 color = albedo.rgb;
#endif

#if VEGETATION_FADE
  output0 = CalculateFogLinear(vec4(color, 1.0), vec4(color, 0.0), v_position.xyz, u_camerapos, $VEGETATION_FADE_START, $VEGETATION_FADE_END);
#endif

#if !VEGETATION_FADE
  output0 = vec4(color, 1.0);
#endif


  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(0.0, 0.9, 0.0, 0.0);
  output4 = vec4(0.0, 0.0, 0.0, 0.0); // TODO: alpha should be aomap
}
