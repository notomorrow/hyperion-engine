#ifndef HYP_LIGHT_SAMPLING_GLSL
#define HYP_LIGHT_SAMPLING_GLSL

#include "shared.inc"
#include "brdf.inc"
#include "scene.inc"

const float lut_size = 64.0;
const float lut_scale = (lut_size - 1.0) / lut_size;
const float lut_bias = (1.0 / lut_size) * 0.5;

// References:
// https://blog.selfshadow.com/publications/s2016-advances/s2016_ltc_rnd.pdf
// https://learnopengl.com/code_viewer_gh.php?code=src/8.guest/2022/7.area_lights/2.multiple_area_lights/7.multi_area_light.fs

float IntegrateEdge(vec3 v1, vec3 v2)
{
    // Using built-in acos() function will result flaws
    // Using fitting result for calculating acos()
    float x = dot(v1, v2);
    float y = abs(x);

    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;

    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;

    return theta_sintheta;
}

vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
    return cross(v1, v2) * IntegrateEdge(v1, v2);
}

vec3 PolygonIrradiance(in vec3 pts[4])
{
    vec3 L0 = normalize(pts[0]);
    vec3 L1 = normalize(pts[1]);
    vec3 L2 = normalize(pts[2]);
    vec3 L3 = normalize(pts[3]);

    float c01 = dot(L0, L1);
    float c12 = dot(L1, L2);
    float c23 = dot(L2, L3);
    float c30 = dot(L3, L0);

    float w01 = (1.5708 - 0.175 * c01) * inversesqrt(c01 + 1.0);
    float w12 = (1.5708 - 0.175 * c12) * inversesqrt(c12 + 1.0);
    float w23 = (1.5708 - 0.175 * c23) * inversesqrt(c23 + 1.0);
    float w30 = (1.5708 - 0.175 * c30) * inversesqrt(c30 + 1.0);

    vec3 L;
    L = cross(L1, -w01 * L0 + w12 * L2);
    L += cross(L3, w30 * L0 + -w23 * L2);

    return L * 0.5;
}

float SphereHorizonCosWarp(float cos_theta, float sin_alpha_sqr)
{
    float sin_alpha = sqrt(sin_alpha_sqr);

    if (cos_theta < sin_alpha)
    {
        cos_theta = max(cos_theta, -sin_alpha);
        cos_theta = pow(sin_alpha + cos_theta, 2.0) / (4.0 * sin_alpha);
    }

    return cos_theta;
}

vec3 CalculateRectLightDiffuse(in Light light, in vec3 P, in vec3 N, in vec3 V)
{
    if (dot(P - light.position_intensity.xyz, light.normal.xyz) <= 0.0) {   
        return vec3(0.0);
    }

    const float half_width = light.area_size.x * 0.5;
    const float half_height = light.area_size.y * 0.5;

    vec3 tangent;
    vec3 bitangent;
    ComputeOrthonormalBasis(light.normal.xyz, tangent, bitangent);
    
    const vec3 right = tangent;
    const vec3 up = bitangent;

    const vec3 p0 = light.position_intensity.xyz + right * half_width + up * half_height;
    const vec3 p1 = light.position_intensity.xyz - right * half_width + up * half_height;
    const vec3 p2 = light.position_intensity.xyz - right * half_width - up * half_height;
    const vec3 p3 = light.position_intensity.xyz + right * half_width - up * half_height;

    const float solid_angle = RectangleSolidAngle(P, p0, p1, p2, p3);

    const float illum = solid_angle * 0.2 * (
        clamp(dot(normalize(p0 - P), N), 0.0, 1.0)
        + clamp(dot(normalize(p1 - P), N), 0.0, 1.0)
        + clamp(dot(normalize(p2 - P), N), 0.0, 1.0)
        + clamp(dot(normalize(p3 - P), N), 0.0, 1.0)
        + clamp(dot(normalize(light.position_intensity.xyz - P), N), 0.0, 1.0)
    );

    return vec3(illum);
}

vec3 CalculateAreaLightRadiance(in Light light, mat3 Minv, in vec3 pts[4], in vec3 P, in vec3 N, in vec3 V, out vec3 lightDir)
{
    vec3 position_to_light = light.position_intensity.xyz - P;

    // construct an orthonormal basis around N
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);

    // Minv = inverse(Minv);
    // rotate area light in (T1, T2, N) basis
    Minv = Minv * transpose(mat3(T1, T2, N));

    // polygon (allocate 4 vertices for clipping)
    vec3 L[4];
    // transform polygon from LTC back to origin Do (cosine weighted)
    L[0] = Minv * (pts[0] - P);
    L[1] = Minv * (pts[1] - P);
    L[2] = Minv * (pts[2] - P);
    L[3] = Minv * (pts[3] - P);

    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    // use tabulated horizon-clipped sphere
    // check if the shading point is behind the light
    vec3 dir = normalize(position_to_light);
    vec3 lightNormal = cross(pts[1].xyz - pts[0].xyz, pts[3].xyz - pts[0].xyz);
    bool behind = (dot(dir, /*light.normal.xyz*/lightNormal) < 0.0);

    vec3 vsum = vec3(0.0);
    vsum += IntegrateEdgeVec(L[0], L[1]);
    vsum += IntegrateEdgeVec(L[1], L[2]);
    vsum += IntegrateEdgeVec(L[2], L[3]);
    vsum += IntegrateEdgeVec(L[3], L[0]);

    // form factor of the polygon in direction vsum
    float len = length(vsum);
    vsum /= len;
    
    if (behind) {
        vsum.z = -vsum.z;
    }

    vec2 uv = vec2(vsum.z * 0.5 + 0.5, len); // range [0, 1]
    uv = uv * lut_scale + vec2(lut_bias);

    // Fetch the form factor for horizon clipping
    float scale = Texture2D(ltc_sampler, ltc_brdf_texture, uv).w;

    float sum = len * scale;
    if (!behind /* && !twoSided*/)
        sum = 0.0;

    // float sum = max((len * len + vsum.z) / (len + 1.0), 0.0);

    // Outgoing radiance (solid angle) for the entire polygon
    return vec3(sum);
}

#endif