#define $SHADOW_BIAS 0.001

uniform sampler2D u_shadowMap[$NUM_SPLITS];
uniform mat4 u_shadowMatrix[$NUM_SPLITS];
uniform float u_shadowSplit[$NUM_SPLITS];

uniform vec2 poissonDisk[16];

bool inRadius(vec3 cam, vec3 world, float radius) {
  if (cam.x >= (world.x - (radius)) && cam.x <= (world.x + (radius))) {
    //if (cam.y >= (world.y - (radius)) && cam.y <= (world.y + (radius))) {
      if (cam.z >= (world.z - (radius)) && cam.z <= (world.z + (radius))) {
        return true;
      }
    //}
  }

  return false;
}

int getShadowMapSplit(vec3 cam, vec3 world) {
  float distanceBuffer = 1.0;;
  //float dist = float(floor(distance(cam, world))) + distanceBuffer;

  if (inRadius(cam, world, u_shadowSplit[0])) {
    return 0;
  } else if (inRadius(cam, world, u_shadowSplit[1])) {
    return 1;
  } else if (inRadius(cam, world, u_shadowSplit[2])) {
    return 2;
  } else if (inRadius(cam, world, u_shadowSplit[3])) {
    return 3;
  }
}

vec3 shadowDebugColor(int shadowSplit) {
  vec3 shadowSplitColors[] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.5, 0.5)
  );

  return shadowSplitColors[shadowSplit];
}

float getShadow(int index, vec3 coord) {
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
#if SHADOWS_PACK_DEPTH
  float depth = unpackDepth(textureSample);
#endif
#if !SHADOWS_PACK_DEPTH
  float depth = textureSample.r;
#endif

#if SHADOWS_VARIANCE
  float moment2 = textureSample.g;

  vec2 moments = vec2(depth, moment2);

  float distance = coord.z;

  if (distance <= depth) {
    return 1.0;
  }

  float variance = moments.y - (moments.x * moments.x);
  variance = max(variance, 0.0000005);
  float d = coord.z - moments.x;
  float shadowPCT = variance / (variance + d*d);
  return shadowPCT;
#endif

#if !SHADOWS_VARIANCE
  return max(step(coord.z-$SHADOW_BIAS, depth), 0.0);
#endif
}

vec3 getShadowCoord(int index, vec3 pos) {
  vec4 shadowPos = u_shadowMatrix[index] * vec4(pos, 1.0);
  shadowPos *= vec4(0.5);
  shadowPos += vec4(0.5);
  shadowPos.xyz /= shadowPos.w;
  return shadowPos.xyz;
}

