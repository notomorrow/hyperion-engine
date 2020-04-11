#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

uniform vec4 u_diffuseColor;
uniform sampler2D u_diffuseMap;
uniform vec3 u_camerapos;

#if SHADOWS
#include "include/shadows.inc"
#endif

#include "include/lighting.inc"

void main() 
{
  vec4 ambient = vec4(vec3(0.3), 1.0);  
  vec4 diffuseTexture = texture2D(u_diffuseMap, v_texcoord0);

  vec3 n = normalize(v_normal.xyz);
  vec3 v = normalize(u_camerapos - v_position.xyz);
  
  float ndotl = max(min(dot(n, env_DirectionalLight.direction), 1.0), 0.0);
  vec4 lighting = vec4(vec3(ndotl), 1.0);
  
  vec3 specular = vec3(max(min(SpecularDirectional(n, u_camerapos.xyz, env_DirectionalLight.direction, u_roughness), 1.0), 0.0));
    
  float fresnel;
  fresnel = max(1.0 - dot(n, v), 0.0);
  fresnel = pow(fresnel, 2.0);
  specular += vec3(fresnel);
  
  vec3 pl_specular = vec3(0.0);
  
  /*for (int i = 0; i < env_NumPointLights; i++) {
    PointLight pl = env_PointLights[i];

    float dist = distance(pl.position, v_position.xyz);

    if (dist < pl.radius) { // TODO: don't even send the PointLight to the shader if this is the case
      vec3 p_direction = normalize(pl.position - v_position.xyz);
      
      float attenuation = 1.0 / (1.0 + 1.0 * pow(dist, 2.0));

      float p_ndotl = max(min(dot(n, p_direction), 1.0), 0.0);
      lighting.rgb += (vec3(p_ndotl) * pl.color.xyz) * (1.0 - (dist / pl.radius)) * attenuation;

      float pl_spec = max(min(SpecularDirectional(n, u_camerapos, p_direction, u_roughness), 1.0), 0.0);

      pl_specular += (vec3(pl_spec) * pl.color.xyz) * (1.0 - (dist / pl.radius)) * attenuation;
    }
  }*/
  
  
#if SHADOWS
  float shadowness = 0.0;
  const float radius = 0.05;
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
  shadowness = mix(0.6, shadowness, ndotl);
#endif

#if !SHADOWS
  float shadowness = 1.0;
#endif

  //specular += pl_specular;
  specular *= u_shininess;


  vec4 diffuse = clamp(lighting + ambient, vec4(0.0), vec4(1.0)) * diffuseTexture;
  //diffuse *= clamp(shadowness, 0.0, 0.5);
  diffuse.rgb *= (1.0 - u_shininess);
  
  gl_FragData[0] = vec4((diffuse + vec4(specular, 1.0)) * vec4(vec3(max(shadowness, 0.5)), 1.0));
  gl_FragData[1] = vec4(n.xyz, 1.0);
  gl_FragData[2] = vec4(v_position.xyz, 1.0);
}