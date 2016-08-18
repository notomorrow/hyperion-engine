#version 330

varying vec4 v_position;
varying vec4 v_normal;
varying vec2 v_texcoord0;

uniform sampler2D u_diffuseTexture;

#include "include/shadows.inc"
#include "include/lighting.inc"

void main() 
{
  float ndotl = max(min(dot(v_normal.xyz, env_DirectionalLight.direction), 1.0), 0.0);
  vec4 lighting = vec4(ndotl, ndotl, ndotl, 1.0);
  
  const float radius = 0.075;
  float shadowness = 0.0;
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      vec2 offset = poissonDisk[x * 4 + y] * radius;
      vec3 shadowCoord = getShadowCoord(v_position.xyz + vec3(offset.x, offset.y, float(x + y)/2.0 * radius));
      shadowness += getShadow(shadowCoord);
    }
  }
  shadowness /= 16.0;
   
  vec4 ambient = vec4(0.1, 0.2, 0.3, 1.0);
  
#if DIFFUSE_MAP
  vec4 diffuseTexture = texture2D(u_diffuseTexture, v_texcoord0);
#endif
#if !DIFFUSE_MAP
  vec4 diffuseTexture = vec4(1.0);
#endif

  vec4 diffuse = (vec4(1.0) * lighting + ambient) * diffuseTexture;
  
  gl_FragColor = vec4((diffuse) * vec4(vec3(max(shadowness, 0.6)), 1.0));
}