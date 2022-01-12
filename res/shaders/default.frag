#version 330 core

#define $PI 3.141592654

in vec4 v_position;
in vec4 v_positionCamspace;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/frag_output.inc"
#include "include/depth.inc"

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

#include "include/parallax.inc"

//Schlick Fresnel
//specular  = the rgb specular color value of the pixel
//VdotH     = the dot product of the camera view direction and the half vector 
vec3 SchlickFresnel(vec3 specular, float VdotH)
{
    return specular + (vec3(1.0, 1.0, 1.0) - specular) * pow(1.0 - VdotH, 5.0);
}

vec3 F_Shlick(float vh,	vec3 F0){
	float fresnelFact = pow(2.0, (-5.55473*vh - 6.98316) * vh);
	return mix(F0, vec3(1.0, 1.0, 1.0), fresnelFact);
}

//Schlick Gaussian Fresnel
//specular  = the rgb specular color value of the pixel
//VdotH     = the dot product of the camera view direction and the half vector 
vec3 SchlickGaussianFresnel(in vec3 specular, in float VdotH)
{
    float sphericalGaussian = pow(2.0, (-5.55473 * VdotH - 6.98316) * VdotH);
    return specular + (vec3(1.0, 1.0, 1.0) - specular) * sphericalGaussian;
}

vec3 SchlickFresnelCustom(vec3 specular, float LdotH)
{
    float ior = 0.25;
    float airIor = 1.000277;
    float f0 = (ior - airIor) / (ior + airIor);
    float max_ior = 2.5;
    f0 = clamp(f0 * f0, 0.0, (max_ior - airIor) / (max_ior + airIor));
    return specular * (f0   + (1.0 - f0) * pow(2.0, (-5.55473 * LdotH - 6.98316) * LdotH));
}
// Smith GGX corrected Visibility
// NdotL        = the dot product of the normal and direction to the light
// NdotV        = the dot product of the normal and the camera view direction
// roughness    = the roughness of the pixel
float SmithGGXSchlickVisibility(float NdotL, float NdotV, float roughness)
{
    float alphaRoughnessSq = roughness * roughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

float NeumannVisibility(float NdotV, float NdotL) 
{
    return min(1.0, NdotL * NdotV / max(0.0001, max(NdotL, NdotV)));
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

  vec4 albedo = u_diffuseColor * diffuseTexture;

  if (albedo.a < 0.1) {
    discard;
  }

  float NdotL = max(0.0, dot(n, lightDir));
	float NdotV = max(0.001, dot(n, viewVector));
	vec3 H = normalize(lightDir + viewVector);
	float NdotH = max(0.001, dot(n, H));
	float LdotH = max(0.001, dot(lightDir, H));
	float VdotH = max(0.001, dot(viewVector, H));
	float HdotV = max(0.001, dot(H, viewVector));

  int mipLevel = 0;//int(floor(roughness * 5));

  vec3 reflectionVector = ReflectionVector(n, v_position.xyz, u_camerapos.xyz);
  vec3 irradianceCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;

  vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;


  float roughnessMix = 1.0 - exp(-(roughness / 1.0 * log(100.0)));
  specularCubemap = mix(specularCubemap, irradianceCubemap, roughnessMix);


  vec3 F0 = vec3(0.04);
  F0 = mix(vec3(1.0), F0, metallic);



  const vec4 c0 = vec4( -1, -0.0275, -0.572, 0.022 );
  const vec4 c1 = vec4( 1, 0.0425, 1.04, -0.04 );
  vec4 r = metallic * c0 + c1;
  float a004 = min( r.x * r.x, exp2( -9.28 * NdotV ) ) * r.x + r.y;
  vec2 AB = vec2( -1.04, 1.04 ) * a004 + r.zw;
  


  vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0.0, 0.0), vec2(1.0, 1.0));
  // retrieve a scale and bias to F0. See [1], Figure 3
  vec2 brdf = texture(BrdfMap, brdfSamplePoint).rg;


  vec3 metallicSpec = mix(vec3(0.04), albedo.rgb, metallic);
  vec3 metallicDiff = mix(albedo.rgb, vec3(0.0), metallic);

  vec3 F = FresnelTerm(metallicSpec, VdotH) * clamp(NdotL, 0.0, 1.0);
  float D = clamp(DistributionGGX(n, H, roughness), 0.0, 1.0);
  //float G = NeumannVisibility(NdotV, NdotL);
  float G = clamp(SmithGGXSchlickVisibility(clamp(NdotL, 0.0, 1.0), clamp(NdotV, 0.0, 1.0), roughness), 0.0, 1.0);
  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= vec3(1.0 - metallic);

  vec3 reflectedLight = vec3(0.0, 0.0, 0.0);
  vec3 diffuseLight = vec3(0.0, 0.0, 0.0);

  float rim = mix(1.0 - roughnessMix * 1.0 /* 'rim' */ * 0.9, 1.0, NdotV);
  vec3 specRef = ((1.0 / rim) * F * G * D) * NdotL;
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

  vec3 color = diffuseLight + reflectedLight; //reflectedLight;//ambientColor + specTerm;

  output0 = vec4(color, albedo.a);
  output1 = vec4(n.xyz, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
}
