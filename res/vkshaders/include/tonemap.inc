#define HDR 1

vec3 TonemapFilmic(vec3 x)
{
  vec3 X = max(vec3(0.0), x - 0.004);
  vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
  
  return pow(result, vec3(2.2));
}

vec3 TonemapUnreal(vec3 x)
{
  return x / (x + 0.155) * 1.019;
}

vec3 Tonemap(vec3 x) {
#if HDR
    #if HDR_TONEMAP_FILMIC
        return TonemapFilmic(x);
    #elif HDR_TONEMAP_UNREAL
        return TonemapUnreal(x);
    #else
        return TonemapFilmic(x);
    #endif
#else
	return x;
#endif
}