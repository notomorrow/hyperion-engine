#if SHADOWS
  float shadowness = 0.0;

#if PSSM_ENABLED
  int shadowSplit = getShadowMapSplit(distance(CameraPosition, position.xyz));
#endif // PSSM_ENABLED

#if !PSSM_ENABLED
  int shadowSplit = 0;
#endif // !PSSM_ENABLED

#if SHADOW_PCF
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            vec2 offset = poissonDisk[x * 4 + y] * $SHADOW_MAP_RADIUS;
            vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz + vec3(offset.x, offset.y, -offset.x));

            shadowness += getShadow(shadowSplit, shadowCoord, NdotL);
        }
    }

    shadowness /= 16.0;
#endif

#if !SHADOW_PCF
    vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz);
    shadowness = getShadow(shadowSplit, shadowCoord, NdotL);
#endif

  vec4 shadowColor = vec4(vec3(shadowness), 1.0);
  //shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), v_position.xyz, u_camerapos, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
  float shadowness = 1.0;
  vec4 shadowColor = vec4(1.0);
#endif