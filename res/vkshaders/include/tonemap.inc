#define HDR 1
//#define HDR_TONEMAP_UNCHARTED 1
#define HDR_TONEMAP_FILMIC 1

#ifndef HDR_EXPOSURE
#define HDR_EXPOSURE 12.0
#endif

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 _TonemapUncharted(vec3 x)
{
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 TonemapUncharted(vec3 x)
{
    x *= HDR_EXPOSURE;  // Hardcoded Exposure Adjustment

    const float ExposureBias = 2.0;
    vec3 curr = _TonemapUncharted(x * ExposureBias);

    vec3 whiteScale = 1.0 / _TonemapUncharted(vec3(W));
    return curr * whiteScale;
}

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
    #elif HDR_TONEMAP_UNCHARTED
        return TonemapUncharted(x);
    #else
        return TonemapFilmic(x);
    #endif
#else
	return x;
#endif
}