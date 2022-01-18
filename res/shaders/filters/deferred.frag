#version 330 core

#define $PI 3.141592654

#include "../include/matrices.inc"
#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/lighting.inc"

#if SHADOWS
#include "../include/shadows.inc"
#endif

in vec2 v_texcoord0;
uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform sampler2D PositionMap;
//uniform sampler2D NormalMap; // included in lighting.inc
uniform sampler2D DataMap;

uniform vec3 CameraPosition;
uniform mat4 InverseViewProjMatrix;

vec3 decodeNormal(vec4 enc)
{
    vec2 fenc = enc.xy*4-2;
    float f = dot(fenc,fenc);
    float g = sqrt(1-f/4);
    vec3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
}

void main()
{
    vec4 albedo = texture(ColorMap, v_texcoord0);
    vec4 data = texture(DataMap, v_texcoord0);
    float depth = texture(DepthMap, v_texcoord0).r;
    vec2 uv = data.xy;
    float metallic = data.z;
    float roughness = data.w;
    vec4 position = vec4(positionFromDepth(InverseViewProjMatrix, v_texcoord0, depth), 1.0);

    vec3 lightDir = normalize(env_DirectionalLight.direction);
    vec3 n = texture(NormalMap, v_texcoord0).rgb * 2.0 - 1.0;
    vec3 viewVector = normalize(CameraPosition - position.xyz);

    float NdotL = max(0.001, dot(n, lightDir));
    float NdotV = max(0.001, dot(n, viewVector));
    vec3 H = normalize(lightDir + viewVector);
    float NdotH = max(0.001, dot(n, H));
    float LdotH = max(0.001, dot(lightDir, H));
    float VdotH = max(0.001, dot(viewVector, H));
    float HdotV = max(0.001, dot(H, viewVector));

#if SHADOWS
    float shadowness = 0.0;
    const float radius = 0.075;
    int shadowSplit = getShadowMapSplit(CameraPosition, position.xyz);

    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            vec2 offset = poissonDisk[x * 4 + y] * radius;
            vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz + vec3(offset.x, offset.y, -offset.x));
            shadowness += getShadow(shadowSplit, shadowCoord);
        }
    }

    shadowness /= 16.0;
    shadowness *= 1.0 - NdotL;

    vec4 shadowColor = vec4(vec3(shadowness), 1.0);
    shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), position.xyz, CameraPosition, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif

#if !SHADOWS
    float shadowness = 1.0;
    vec4 shadowColor = vec4(1.0);
#endif

    vec3 reflectionVector = ReflectionVector(n, position.xyz, CameraPosition);

    vec3 diffuseCubemap = texture(env_GlobalIrradianceCubemap, n).rgb;
    vec3 specularCubemap = texture(env_GlobalCubemap, reflectionVector).rgb;
    vec3 blurredSpecularCubemap = texture(env_GlobalIrradianceCubemap, reflectionVector).rgb;

    float roughnessMix = 1.0 - exp(-(roughness / 1.0 * log(100.0)));
    specularCubemap = mix(specularCubemap, blurredSpecularCubemap, roughnessMix);

    vec3 F0 = vec3(0.04);
    F0 = mix(vec3(1.0), F0, metallic);

    vec2 AB = BRDFMap(NdotV, metallic);

    vec3 metallicSpec = mix(vec3(0.04), albedo.rgb, metallic);
    vec3 metallicDiff = mix(albedo.rgb, vec3(0.0), metallic);

    vec3 F = FresnelTerm(metallicSpec, VdotH) * clamp(NdotL, 0.0, 1.0);
    float D = clamp(DistributionGGX(n, H, roughness), 0.0, 1.0);
    float G = clamp(SmithGGXSchlickVisibility(clamp(NdotL, 0.0, 1.0), clamp(NdotV, 0.0, 1.0), roughness), 0.0, 1.0);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= vec3(1.0 - metallic);

    vec3 reflectedLight = vec3(0.0, 0.0, 0.0);
    vec3 diffuseLight = vec3(0.0, 0.0, 0.0);

    float rim = mix(1.0 - roughnessMix * 1.0 * 0.9, 1.0, NdotV);
    vec3 specRef = vec3((1.0 / max(rim, 0.0001)) * F * G * D) * NdotL;
    reflectedLight += specRef;

    vec3 ibl = min(vec3(0.99), FresnelTerm(metallicSpec, NdotV) * AB.x + AB.y);
    reflectedLight += ibl * specularCubemap;

    vec3 diffRef = vec3((vec3(1.0) - F) * (1.0 / $PI) * NdotL);
    diffuseLight += diffRef;
    diffuseLight += EnvRemap(Irradiance(n)) * (1.0 / $PI);
    diffuseLight *= metallicDiff;

    vec3 result = diffuseLight + reflectedLight * shadowColor.rgb;

    output0 = vec4(result, 1.0);
}
