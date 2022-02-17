vec3 tonemapFilmic(vec3 x) {
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  return pow(result, vec3(2.2));
}
vec3 tonemapUnreal(vec3 x) {
  return x / (x + 0.155) * 1.019;
}

vec3 tonemap(vec3 x) {
#if HDR
#if HDR_TONEMAP_FILMIC
	return tonemapFilmic(x);
#endif
#if HDR_TONEMAP_UNREAL
	return tonemapUnreal(x);
#endif
#endif

#if !HDR
	return x;
#endif
}