
#define PI 3.14159265359

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    // float a = roughness*roughness;
    // float a2 = a*a;
    // float NdotH = max(dot(N, H), 0.0);
    // float NdotH2 = NdotH*NdotH;

    // float nom   = a2;
    // float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    // denom = PI * denom * denom;

    // return nom / denom;


    // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"

    // In mediump, there are two problems computing 1.0 - NoH^2
    // 1) 1.0 - NoH^2 suffers floating point cancellation when NoH^2 is close to 1 (highlights)
    // 2) NoH doesn't have enough precision around 1.0
    // Both problem can be fixed by computing 1-NoH^2 in highp and providing NoH in highp as well

    // However, we can do better using Lagrange's identity:
    //      ||a x b||^2 = ||a||^2 ||b||^2 - (a . b)^2
    // since N and H are unit vectors: ||N x H||^2 = 1.0 - NoH^2
    // This computes 1.0 - NoH^2 directly (which is close to zero in the highlights and has
    // enough precision).
    // Overall this yields better performance, keeping all computations in mediump
//     const float NoH = dot(N, H);
// #if defined(TARGET_MOBILE)
//     vec3 NxH = cross(N, H);
//     float oneMinusNoHSquared = dot(NxH, NxH);
// #else
//     float oneMinusNoHSquared = 1.0 - NoH * NoH;
// #endif

//     float a = NoH * roughness;
//     float k = roughness / (oneMinusNoHSquared + a * a);
//     float d = k * k * (1.0 / PI);
//     return d;


    float NdotH = max(dot(N, H), 0.0);

    const float r2 = HYP_FMATH_SQR(roughness);
    const float d = (NdotH * r2 - NdotH) * NdotH + 1.0;
    return r2 / (d * d * HYP_FMATH_PI);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
    // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
    float a2 = roughness * roughness;
    // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
    float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
    float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
    float v = 0.5 / max(lambdaV + lambdaL, 0.00001);
    // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
    // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
    // clamp to the maximum value representable in mediump
    return v;
}


float CookTorranceG(float NdotL, float NdotV, float LdotH, float NdotH)
{
    return min(1, 2 * (NdotH / LdotH) * min(NdotL, NdotV));
}

vec3 SchlickFresnel(vec3 f0, vec3 f90, float u)
{
    const float Fc = pow(1 - u, 5.0);
    return f0 * (1.0 - Fc) + f90 * Fc;

    // return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec4 SchlickFresnel(vec4 f0, vec4 f90, float u)
{
    const float Fc = pow(1 - u, 5.0);
    return f0 * (1.0 - Fc) + f90 * Fc;

    // return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

vec4 SchlickFresnelRoughness(vec4 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(vec4(1.0 - roughness), f0) - f0) * pow(1.0 - u, 5.0);
}

vec3 SchlickFresnelRoughness(vec3 f0, float roughness, float u)
{
    // const float Fc = pow(1 - u, 5.0);
    // return f0 * (1.0 - Fc) + f90 * Fc;

    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - u, 5.0);
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
vec3 EnvBRDFApprox(vec3 specular_color, float roughness, float ndotv)
{
	const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return specular_color * AB.x + AB.y;
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
vec4 EnvBRDFApprox(vec4 specular_color, float roughness, float ndotv)
{
	const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return specular_color * AB.x + AB.y;
}

vec2 BRDFMap(float roughness, float NdotV)
{
    const vec4 c0 = vec4( -1, -0.0275, -0.572, 0.022 );
    const vec4 c1 = vec4( 1, 0.0425, 1.04, -0.04 );
    vec4 r = roughness * c0 + c1;
    float a004 = min( r.x * r.x, exp2( -9.28 * NdotV ) ) * r.x + r.y;
    return vec2( -1.04, 1.04 ) * a004 + r.zw;
}

float sqr(float x)
{
    return x * x;
}

/* From Google Filament */

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid in the range -1..1.
 */
float acosFast(float x)
{
    // Lagarde 2014, "Inverse trigonometric functions GPU optimization for AMD GCN architecture"
    // This is the approximation of degree 1, with a max absolute error of 9.0x10^-3
    float y = abs(x);
    float p = -0.1565827 * y + 1.570796;
    p *= sqrt(1.0 - y);
    return x >= 0.0 ? p : PI - p;
}

/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid only in the range 0..1.
 */
float acosFastPositive(float x)
{
    float p = -0.1565827 * x + 1.570796;
    return p * sqrt(1.0 - x);
}

float sphericalCapsIntersection(float cosCap1, float cosCap2, float cosDistance)
{
    // Oat and Sander 2007, "Ambient Aperture Lighting"
    // Approximation mentioned by Jimenez et al. 2016
    float r1 = acosFastPositive(cosCap1);
    float r2 = acosFastPositive(cosCap2);
    float d  = acosFast(cosDistance);

    // We work with cosine angles, replace the original paper's use of
    // cos(min(r1, r2)) with max(cosCap1, cosCap2)
    // We also remove a multiplication by 2 * PI to simplify the computation
    // since we divide by 2 * PI in computeBentSpecularAO()

    if (min(r1, r2) <= max(r1, r2) - d) {
        return 1.0 - max(cosCap1, cosCap2);
    } else if (r1 + r2 <= d) {
        return 0.0;
    }

    float delta = abs(r1 - r2);
    float x = 1.0 - clamp((d - delta) / max(r1 + r2 - delta, 1e-4), 0.0, 1.0);
    // simplified smoothstep()
    float area = sqr(x) * (-2.0 * x + 3.0);
    return area * (1.0 - max(cosCap1, cosCap2));
}

float SpecularAO_Cones(vec3 bentNormal, float visibility, float roughness, vec3 shading_reflected)
{
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"

    // aperture from ambient occlusion
    float cosAv = sqrt(1.0 - visibility);
    // aperture from roughness, log(10) / log(2) = 3.321928
    float cosAs = exp2(-3.321928 * sqr(roughness));
    // angle betwen bent normal and reflection direction
    float cosB  = dot(bentNormal, shading_reflected);

    // Remove the 2 * PI term from the denominator, it cancels out the same term from
    // sphericalCapsIntersection()
    float ao = sphericalCapsIntersection(cosAv, cosAs, cosB) / (1.0 - cosAs);
    // Smoothly kill specular AO when entering the perceptual roughness range [0.1..0.3]
    // Without this, specular AO can remove all reflections, which looks bad on metals
    return mix(1.0, ao, smoothstep(0.01, 0.09, roughness));
}

float SpecularAO_Lagarde(float NoV, float visibility, float roughness)
{
    // Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
    return clamp(pow(NoV + visibility, exp2(-16.0 * roughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
}

vec3 GTAOMultiBounce(float visibility, const vec3 albedo) {
    // Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion"
    vec3 a =  2.0404 * albedo - 0.3324;
    vec3 b = -4.7951 * albedo + 0.6417;
    vec3 c =  2.7552 * albedo + 0.6903;

    return max(vec3(visibility), ((visibility * a + b) * visibility + c) * visibility);
}