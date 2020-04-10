uniform sampler2D u_shadowMap[$NUM_SPLITS];
uniform mat4 u_shadowMatrix[$NUM_SPLITS];
uniform float u_shadowSplit[$NUM_SPLITS];

uniform vec2 poissonDisk[16];

int getShadowMapSplit(vec3 cam, vec3 world) {
  float len = length(cam - world);
  if (len <= u_shadowSplit[0]) {
    return 0;
  } else if (len <= u_shadowSplit[1]) {
    return 1;
  } else if (len <= u_shadowSplit[2]) {
    return 2;
  } else {
    return 3;
  }
}

float getShadow(int index, vec3 coord) {
  float bias = 0.0015;
  float depth = texture2D(u_shadowMap[index], coord.xy).r;
  return max(step(coord.z-bias, depth), 0.0);
}

vec3 getShadowCoord(int index, vec3 pos) {
  vec4 shadowPos = u_shadowMatrix[index] * vec4(pos, 1.0);
  shadowPos *= vec4(0.5);
  shadowPos += vec4(0.5);
  return shadowPos.xyz;
}