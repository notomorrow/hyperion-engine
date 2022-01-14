uniform sampler2D u_shadowMap[$NUM_SPLITS];
uniform mat4 u_shadowMatrix[$NUM_SPLITS];
uniform float u_shadowSplit[$NUM_SPLITS];

uniform vec2 poissonDisk[16];

bool inRadius(vec3 cam, vec3 world, float radius) {
  if (cam.x >= (world.x - (radius)) && cam.x <= (world.x + (radius))) {
    if (cam.y >= (world.y - (radius)) && cam.y <= (world.y + (radius))) {
      //if (cam.z >= (world.z - (radius)) && cam.z <= (world.z + (radius))) {
        return true;
      //}
    }
  }

  return false;
}

int getShadowMapSplit(vec3 cam, vec3 world) {
  float distanceBuffer = 1.0;
  float dist = float(floor(distance(cam, world))) + distanceBuffer;

  if (dist < u_shadowSplit[0]) {
    return 0;
  } else if (dist < u_shadowSplit[1]) {
    return 1;
  } else if (dist < u_shadowSplit[2]) {
    return 2;
  } else if (dist < u_shadowSplit[3]) {
    return 3;
  }
}

float getShadow(int index, vec3 coord) {
  float bias = 0.01;
  vec4 textureSample = vec4(0.0);

  if (index == 0) {
	  textureSample = texture(u_shadowMap[0], coord.xy);
	} else if (index == 1) {
		textureSample = texture(u_shadowMap[1], coord.xy);
	} else if (index == 2) {
		textureSample = texture(u_shadowMap[2], coord.xy);
	} else {
		textureSample = texture(u_shadowMap[3], coord.xy);
	}
#if PACK_DEPTH
  float depth = unpackDepth(textureSample);
#endif
#if !PACK_DEPTH
  float depth = textureSample.r;
#endif
  return max(step(coord.z-bias, depth), 0.0);
}

vec3 getShadowCoord(int index, vec3 pos) {
  vec4 shadowPos = u_shadowMatrix[index] * vec4(pos, 1.0);
  shadowPos *= vec4(0.5);
  shadowPos += vec4(0.5);
  return shadowPos.xyz;
}
