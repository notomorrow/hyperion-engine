#if NOOP
//VCT_ENABLED

#version 430

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.08

in vec4 v_position;
in vec4 v_ndc;
in vec4 v_voxelPosition;
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
uniform vec3 u_probePos;
uniform float u_scale;
uniform sampler3D tex[6];
uniform vec3 VoxelSceneScale;
uniform vec3 VoxelProbePosition;

#define $SPECULAR_CONE_SIZE 0.1
#define $DIFFUSE_CONE_SIZE 0.5

uniform mat4 WorldToVoxelTexMatrix;

vec4 voxelFetch(vec3 pos, vec3 dir, float lod)
{
	vec4 sampleX =
		dir.x < 0.0
		? textureLod(tex[0], pos, lod)
		: textureLod(tex[1], pos, lod);
	
	vec4 sampleY =
		dir.y < 0.0
		? textureLod(tex[2], pos, lod)
		: textureLod(tex[3], pos, lod);
	
	vec4 sampleZ =
		dir.z < 0.0
		? textureLod(tex[4], pos, lod)
		: textureLod(tex[5], pos, lod);
	
	vec3 sampleWeights = abs(dir);
	float invSampleMag = 1.0 / (sampleWeights.x + sampleWeights.y + sampleWeights.z + .0001);
	sampleWeights *= invSampleMag;
	
	vec4 filtered = 
		sampleX * sampleWeights.x
		+ sampleY * sampleWeights.y
		+ sampleZ * sampleWeights.z;
		
		//if (dir.x < 0.0) {
		//	return textureLod(tex[0], pos, lod);
		//} else {
		//	return textureLod(tex[1], pos, lod);
		//}
		vec4 negX = textureLod(tex[0], pos, lod);
		vec4 posX = textureLod(tex[1], pos, lod);
		
		//if (dir.y < 0.0) {
		//	return textureLod(tex[2], pos, lod);
		//} else {
		///	return vec4(0.0);
		//}
		
		//return mix(negX, posX, clamp(dir.x*0.5+0.5, 0.0, 1.0));
		
	
	return filtered;
}
// origin, dir, and maxDist are in texture space
// dir should be normalized
// coneRatio is the cone diameter to height ratio (2.0 for 90-degree cone)
vec4 voxelTraceCone(float minVoxelDiameter, vec3 origin, vec3 dir, float coneRatio, float maxDist)
{
	float minVoxelDiameterInv = 1.0/minVoxelDiameter;
	vec3 samplePos = origin;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(1.0, 1.0, 1.0, 1.0)*0.2;
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		vec3 samplePos = origin + dir * dist;
		vec4 sampleValue = voxelFetch(samplePos, -dir, sampleLOD);
		sampleValue = mix(sampleValue,fadeCol, clamp(dist/maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}
	return accum;
}
void main() 
{
	float voxelImageSize = float($VOXEL_MAP_SIZE);
	float halfVoxelImageSize = voxelImageSize * 0.5;

	vec3 eyeToFragment = normalize(v_position.xyz - u_camerapos);
	vec3 reflectionDir = reflect(eyeToFragment, normalize(v_normal.xyz));
	
	vec4 albedo = u_diffuseColor;
	
	if (HasDiffuseMap == 1) {
		albedo *= texture(DiffuseMap, v_texcoord0);
	}
		
	vec3 lambert = ComputeDirectionalLight(env_DirectionalLight, v_normal.xyz, eyeToFragment, v_position.xyz, albedo.rgb, 0.0, 0.5, 0.5);
	vec4 specular = vec4(0.0, 0.0, 0.0, 1.0);// = calcDirectionalLightSpec(normalize(directionalLight.direction), u_specularFactor, normalize(v_normals.xyz), u_cameraPosition, v_position, C_albedo);
	vec4 imageColor = u_diffuseColor;
	//if (B_hasDiffuseMap > 0) {
	//	imageColor.rgb *= texture2D(T_diffuse, texcoord0).rgb;
	//}	
	vec4 coneTraceRes;
	float dist = distance(u_camerapos, v_position.xyz);
	float maxDist = voxelImageSize/u_scale;
	//if (dist < maxDist) {
		vec3 vPos = (vec3((v_voxelPosition.xyz-VoxelProbePosition) * 0.1)+vec3(halfVoxelImageSize))/vec3(voxelImageSize);
		//vec4 voxelPosition = u_projMatrix * u_viewMatrix * vec4(v_position.xyz, 1.0);//WorldToVoxelTexMatrix * vec4(v_position.xyz, 1.0);
		//voxelPosition.xyz /= voxelPosition.w;
		//vec3 vPos = voxelPosition.xyz;
		
		
		float rangeMin = 0.02;
		float rangeMax = 0.1;
		float level = clamp(dist/maxDist*0.5, rangeMin, rangeMax);
		#define $SPECULAR_MAX_LEN 20.0
		vec4 reflection = voxelTraceCone(1.0/voxelImageSize, vPos, normalize(reflectionDir), $SPECULAR_CONE_SIZE, $SPECULAR_MAX_LEN);
		#define $DIFFUSE_MAX_LEN 0.1
		coneTraceRes += voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz), $DIFFUSE_CONE_SIZE, $DIFFUSE_MAX_LEN);
	    coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz+v_tangent), $DIFFUSE_CONE_SIZE, $DIFFUSE_MAX_LEN);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz-v_tangent), $DIFFUSE_CONE_SIZE, $DIFFUSE_MAX_LEN);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz+v_bitangent), $DIFFUSE_CONE_SIZE, $DIFFUSE_MAX_LEN);
		coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, vPos, normalize(v_normal.xyz-v_bitangent), $DIFFUSE_CONE_SIZE, $DIFFUSE_MAX_LEN);
		specular += reflection;////mix(reflection, vec4(0.0), clamp(dist/maxDist, 0.0, 1.0));//+vec4(ambientLight.color.rgb, 1.0);
	//}
	
	output0 = vec4( coneTraceRes.rgb * 0.2 + reflection.rgb + lambert, 1.0);//(lambert)*imageColor + (specular) ;
	
}

#endif


#if !NOOP

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

  vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;

#if PROBE_ENABLED
  vec3 blurredSpecularCubemap = SampleEnvProbe(env_GlobalIrradianceCubemap, n, v_position.xyz, u_camerapos).rgb;
  vec3 specularCubemap = SampleEnvProbe(env_GlobalCubemap, n, v_position.xyz, u_camerapos).rgb;
#endif

#if !PROBE_ENABLED
  vec3 reflectionVector = ReflectionVector(n, v_position.xyz, u_camerapos);
  vec3 blurredSpecularCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;
  vec3 specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;
#endif

#if VCT_ENABLED
  //testing
  vec4 vctSpecular = VCTSpecular(v_position.xyz, n.xyz, u_camerapos);
  vec4 vctDiffuse = VCTDiffuse(v_position.xyz, n.xyz, u_camerapos, v_tangent, v_bitangent);
  specularCubemap = vctSpecular.rgb;
  diffuseCubemap = vctDiffuse.rgb;
#endif

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
  diffuseLight += diffRef;
  diffuseLight += diffuseCubemap.rgb;
  diffuseLight *= metallicDiff;

  vec3 color = diffuseLight + reflectedLight * shadowColor.rgb;
  
#if VCT_ENABLED

  output0 = vec4(color, 1.0);//voxelFetch(decodeVoxelPosition(v_position.xyz), viewVector, 0);
#endif

#if !VCT_ENABLED
  output0 = vec4(color, 1.0);
#endif
#endif

#if DEFERRED
 output0 = albedo;
#endif

  output1 = vec4(n * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(metallic, roughness, 0.0, 1.0);
  output4 = vec4(0.0, 0.0, 0.0, 0.0); // TODO: alpha should be aomap
}

#endif