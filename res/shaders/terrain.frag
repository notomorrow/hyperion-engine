//#version 330 core
#version 430 core

#define $PI 3.141592654
#define $ALPHA_DISCARD 0.2

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

#define $TRIPLANAR_BLENDING 1

uniform sampler2D BaseTerrainColorMap;
uniform sampler2D BaseTerrainNormalMap;
uniform sampler2D BaseTerrainParallaxMap;
uniform sampler2D BaseTerrainAoMap;
uniform int HasBaseTerrainColorMap;
uniform int HasBaseTerrainNormalMap;
uniform int HasBaseTerrainParallaxMap;
uniform int HasBaseTerrainAoMap;
uniform float BaseTerrainScale = 1.0;

uniform sampler2D Terrain1ColorMap;
uniform sampler2D Terrain1NormalMap;
uniform sampler2D Terrain1ParallaxMap;
uniform sampler2D Terrain1AoMap;
uniform int HasTerrain1ColorMap;
uniform int HasTerrain1NormalMap;
uniform int HasTerrain1ParallaxMap;
uniform int HasTerrain1AoMap;
uniform float Terrain1Scale = 1.0;

uniform sampler2D Terrain2ColorMap;
uniform sampler2D Terrain2NormalMap;
uniform sampler2D Terrain2ParallaxMap;
uniform sampler2D Terrain2AoMap;
uniform int HasTerrain2ColorMap;
uniform int HasTerrain2NormalMap;
uniform int HasTerrain2ParallaxMap;
uniform int HasTerrain2AoMap;
uniform float Terrain2Scale = 1.0;

uniform sampler2D SplatMap;
uniform int HasSplatMap;
uniform float SplatMapScale = 1.0;

#define $SIMPLE_SPLAT_BLEND 1

vec4 encodeNormal(vec3 n)
{
    float p = sqrt(n.z*8+8);
    return vec4(n.xy/p + 0.5,0,0);
}

struct TerrainRegion {
    vec4 color;
    vec3 normal;
    float ao;
    vec3 parallax;
};

TerrainRegion UpdateTerrainRegionTextures(sampler2D colorMap, sampler2D normalMap, sampler2D aoMap, sampler2D parallaxMap, float scale, vec3 blending)
{
    TerrainRegion region;

    // === flat terrain level ===
    #if TRIPLANAR_BLENDING

      vec4 flat_xaxis = texture(colorMap, v_position.yz * scale);
      vec4 flat_yaxis = texture(colorMap, v_position.xz * scale);
      vec4 flat_zaxis = texture(colorMap, v_position.xy * scale);
      // blend the results of the 3 planar projections.
      region.color = flat_xaxis * blending.x + flat_yaxis * blending.y + flat_zaxis * blending.z;

      vec3 normal_xaxis = texture(normalMap, v_position.yz * scale).rgb * 2.0 - vec3(1.0);
      vec3 normal_yaxis = texture(normalMap, v_position.xz * scale).rgb * 2.0 - vec3(1.0);
      vec3 normal_zaxis = texture(normalMap, v_position.xy * scale).rgb * 2.0 - vec3(1.0);

      region.normal = normal_xaxis * blending.x + normal_yaxis * blending.y + normal_zaxis * blending.z;
    #endif

    #if !TRIPLANAR_BLENDING
      region.color = texture(colorMap, v_texcoord0 * scale);
      region.normal = texture(normalMap, v_texcoord0 * scale).rgb * 2.0 - vec3(1.0);
    #endif

    region.ao = 1.0;
    
    return region;
}

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

#if TRIPLANAR_BLENDING
    // in wNorm is the world-space normal of the fragment
    vec3 blending = abs(n);
    blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
    float b = (blending.x + blending.y + blending.z);
    blending /= vec3(b, b, b);
#endif

#if !TRIPLANAR_BLENDING
    vec3 blending = vec3(0.0);
#endif

  TerrainRegion regions[4];
  

  
  regions[0] = UpdateTerrainRegionTextures(BaseTerrainColorMap, BaseTerrainNormalMap, BaseTerrainAoMap, BaseTerrainParallaxMap, BaseTerrainScale, blending);

  regions[1] = (HasTerrain1ColorMap == 1)
    ? UpdateTerrainRegionTextures(Terrain1ColorMap, Terrain1NormalMap, Terrain1AoMap, Terrain1ParallaxMap, Terrain1Scale, blending)
    : regions[0];
  regions[2] = (HasTerrain2ColorMap == 1)
    ? UpdateTerrainRegionTextures(Terrain2ColorMap, Terrain2NormalMap, Terrain2AoMap, Terrain2ParallaxMap, Terrain2Scale, blending)
    : regions[0];

  // will be blended
  vec4 finalColor = regions[0].color;
  vec3 finalNormal = regions[0].normal;
  float finalAo = regions[0].ao;
  vec3 finalParallax = regions[0].parallax;
  
  vec4 splatMap = vec4(1.0, 0.0, 0.0, 0.0);

  if (HasSplatMap == 1) {
      splatMap = texture(SplatMap, texCoords);
      
      finalColor *= splatMap.r;
      
      finalColor = mix(finalColor, regions[1].color, splatMap.g);
      finalColor = mix(finalColor, regions[2].color, splatMap.b);
      
      finalNormal = mix(finalNormal, regions[1].normal, splatMap.g);
      finalNormal = mix(finalNormal, regions[2].normal, splatMap.b);
      
      finalAo = mix(finalAo, regions[1].ao, splatMap.g);
      finalAo = mix(finalAo, regions[2].ao, splatMap.b);
      
      finalParallax = mix(finalParallax, regions[1].parallax, splatMap.g);
      finalParallax = mix(finalParallax, regions[2].parallax, splatMap.b);
  }

//#if PARALLAX_MAPPING
//  // TODO: pick best parallax text based on highest float value of rgb?
//    texCoords = ParallaxMappingTexture(BaseTerrainParallaxMap, texCoords, normalize(tangentFragPos - tangentViewPos));

//    if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0) {
//      texCoords = v_texcoord0;
//    }
//#endif
  
  ao = 1.0 - finalAo;

  vec4 albedo = u_diffuseColor * finalColor;

#if NORMAL_MAPPING
  n = normalize(v_tbn * finalNormal);
#endif

  output0 = vec4(albedo.rgb, 1.0);
  output1 = vec4(normalize(n) * 0.5 + 0.5, 1.0);
  output2 = vec4(v_position.xyz, 1.0);
  output3 = vec4(metallic, roughness, 0.0, 1.0);
  output4 = vec4(0.0, 0.0, 0.0, ao);
  output5 = vec4(normalize(v_tangent.xyz) * 0.5 + 0.5, 1.0);
  output6 = vec4(normalize(v_bitangent.xyz) * 0.5 + 0.5, 1.0);
}