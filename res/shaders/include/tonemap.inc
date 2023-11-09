#ifndef HYP_TONEMAP_GLSL
#define HYP_TONEMAP_GLSL

#define HDR 1
//#define HDR_TONEMAP_UNCHARTED 1
//#define HDR_TONEMAP_FILMIC 1
#define HDR_TONEMAP_UNREAL 1
//#define HDR_TONEMAP_REINHARD 1

#ifndef HDR_EXPOSURE
    #define HDR_EXPOSURE 9.0
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
    //x = 1.0 - exp(-1.0 * x);

    vec3 X = max(vec3(0.0), x - 0.004);
    return (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
}

vec3 TonemapUnreal(vec3 x)
{
    return x / (x + 0.155) * 1.019;
}

float GetLuminance(vec3 x)
{
    return dot(x, vec3(0.2126, 0.7152, 0.0722));
}

vec3 ChangeLuminance(vec3 x, float new_luminance)
{
    float luminance = GetLuminance(x);

    return x * (new_luminance / max(luminance, 0.00001));
}

vec3 TonemapReinhard(vec3 x)
{
    const float max_white_l = 100.0;
    float l_old = GetLuminance(x);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / max(1.0f + l_old, 0.00001);
    return ChangeLuminance(x, l_new);
}

vec3 TonemapReinhardSimple(vec3 x)
{
    return x / (vec3(1.0) + x);
}

vec3 ReverseTonemapReinhardSimple(vec3 x)
{
    return x / (vec3(1.0) - x);
}

vec3 Tonemap(vec3 x) {
#if defined(HDR) && HDR
    #if HDR_TONEMAP_FILMIC
        return TonemapFilmic(x);
    #elif HDR_TONEMAP_UNREAL
        return TonemapUnreal(x);
    #elif HDR_TONEMAP_UNCHARTED
        return TonemapUncharted(x);
    #elif HDR_TONEMAP_REINHARD
        return TonemapReinhard(x);
    #else
        return TonemapReinhard(x);
    #endif
#else
	return x;
#endif
}

#endif