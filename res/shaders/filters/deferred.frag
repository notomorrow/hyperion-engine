#version 330 core

#define $PI 3.141592654

#include "../include/matrices.inc"
#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/lighting.inc"
#include "../include/material.inc"
#include "../include/parallax.inc"
#include "../include/tonemap.inc"

#include "../include/shadows.inc"

in vec2 v_texcoord0;
uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform sampler2D PositionMap;
uniform sampler2D DataMap;
uniform sampler2D TangentMap;
uniform sampler2D BitangentMap;

uniform sampler3D LightVolumeMap;

uniform sampler2D SSLightingMap;
uniform int HasSSLightingMap;

uniform vec3 CameraPosition;
uniform vec2 CameraNearFar;
uniform mat4 InverseViewProjMatrix;

float linearDepth(mat4 projMatrix, float depth)
{
    return projMatrix[3][2] / (depth * projMatrix[2][3] - projMatrix[2][2]);
}

#include "../post/modules/base.glsl"
#include "../post/modules/ssr.glsl"

#define $DIRECTIONAL_LIGHTING 1

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

float SchlickFresnel2(float u)
{
    float m = clamp(1-u, 0, 1);
    float m2 = m*m;
    return m2*m2*m; // pow(m,5)
}

float GTR1(float NdotH, float a)
{
    if (a >= 1) return 1.0/$PI;
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return (a2-1) / (PI*log(a2)*t);
}

float GTR2(float NdotH, float a)
{
    float a2 = a*a;
    float t = 1 + (a2-1)*NdotH*NdotH;
    return a2 / (PI * t*t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
    return 1.0 / ($PI * ax*ay * sqr( sqr(HdotX/ax) + sqr(HdotY/ay) + NdotH*NdotH ));
}

float smithG_GGX(float NdotV, float alphaG)
{
    float a = alphaG*alphaG;
    float b = NdotV*NdotV;
    return 1.0 / (NdotV + sqrt(a + b - a*b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
    return 1.0 / max(NdotV + sqrt( sqr(VdotX*ax) + sqr(VdotY*ay) + sqr(NdotV) ), 0.001);
}

vec3 mon2lin(vec3 x)
{
    return vec3(pow(x[0], 2.2), pow(x[1], 2.2), pow(x[2], 2.2));
}


#define $VOLUMETRIC_LIGHTING_ENABLED 1
#define $VOLUMETRIC_LIGHTING_STEPS 64
#define $VOLUMETRIC_LIGHTING_MIE_G -0.995
#define $VOLUMETRIC_LIGHTING_FADE_START 0.95
#define $VOLUMETRIC_LIGHTING_FADE_END 0.9
#define $VOLUMETRIC_LIGHTING_INTENSITY 0.9

struct VolumetricLight {
    float scattering;
    float extinction;
    float range;
};

float VolumetricLightDensity(vec3 position)
{
    // TODO: noise
    return 1.0;
}

float VolumetricLightAttenuation(vec3 position, float NdotL)
{
    vec3 shadowCoord = getShadowCoord(0, position);
    float shadow = getShadow(0, shadowCoord, NdotL);
    
    return shadow;
}

float VolumetricLightMieScattering(float fCos, float fCos2, float g, float g2)
{
    return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + fCos2) / pow(1.0 + g2 - 2.0*g*fCos, 1.5);
}

vec4 VolumetricLightRaymarch(vec2 uv, vec3 rayStart, vec3 rayDir, float rayLength)
{
    VolumetricLight volumetricLight;
    volumetricLight.scattering = 1.0;
    volumetricLight.extinction = 1.0;
    
    float stepSize = rayLength / $VOLUMETRIC_LIGHTING_STEPS;
    vec3 rayDeltaStep = rayDir * stepSize;

    vec3 currentPosition = rayStart + rayDeltaStep;
    
    float volumetric = 0.0;
    
    float extinction = 0.0; // for directional light
    float cosAngle = dot(env_DirectionalLight.direction, -rayDir);
    
    for (int i = 0; i < $VOLUMETRIC_LIGHTING_STEPS; i++) {
        float atten = VolumetricLightAttenuation(currentPosition, cosAngle);
        //float density = VolumetricLightDensity(currentPosition);
        //float scattering = volumetricLight.scattering * stepSize * density;
        //extinction += volumetricLight.extinction * stepSize * density;
        
        float light = atten;//atten * scattering * density * exp(-extinction);
        
        volumetric += light;
        
        currentPosition += rayDeltaStep;
    }
    
    volumetric /= float($VOLUMETRIC_LIGHTING_STEPS);

    volumetric *= clamp(VolumetricLightMieScattering(cosAngle, cosAngle*cosAngle, $VOLUMETRIC_LIGHTING_MIE_G, $VOLUMETRIC_LIGHTING_MIE_G * $VOLUMETRIC_LIGHTING_MIE_G), 0.0, 1.0);
    volumetric = max(0.0, volumetric);
    //volumetric *= exp(-extinction);
    
    return env_DirectionalLight.color * volumetric * $VOLUMETRIC_LIGHTING_INTENSITY;
}


void main()
{
    vec4 albedo = texture(ColorMap, v_texcoord0);
    vec4 data = texture(DataMap, v_texcoord0);
    float depth = texture(DepthMap, v_texcoord0).r;

    float metallic = clamp(data.r, 0.05, 0.99);
    float roughness = clamp(data.g, 0.05, 0.99);
    float performLighting = data.a;
    float ao = 1.0;
    vec4 gi = vec4(0.0);
	vec3 irradiance = vec3(1.0);

    vec3 N = (texture(NormalMap, v_texcoord0).rgb * 2.0 - 1.0);
	vec3 tangent = texture(TangentMap, v_texcoord0).rgb * 2.0 - 1.0;
	vec3 bitangent = texture(BitangentMap, v_texcoord0).rgb * 2.0 - 1.0;
	mat3 tbn = mat3( normalize(tangent), normalize(bitangent), N );
	
    vec4 position = vec4(positionFromDepth(InverseViewProjMatrix, v_texcoord0, depth), 1.0);
    vec3 L = normalize(env_DirectionalLight.direction);
    vec3 V = normalize(CameraPosition-position.xyz);

    if (performLighting < 0.9) {
        output0 = vec4(albedo.rgb, 1.0);
        return;
    }

    if (HasSSLightingMap == 1) {
        vec4 ssl = texture(SSLightingMap, v_texcoord0);
        //gi = ssl.rgb;
		
		// if ssl.a == 0, do not use AO from ao map (hence why we invert it)
		// else, just use our existing ao
        //ao = mix(ao, 1.0 - ssl.a, ssl.a);
        ao = 1.0 - ssl.a;
    }

#if DIRECTIONAL_LIGHTING

	
	vec3 X = tangent;
	vec3 Y = bitangent;

    float NdotL = max(0.001, dot(N, L));
    float NdotV = max(0.001, dot(N, V));
    vec3 H = normalize(L + V);
    float NdotH = max(0.001, dot(N, H));
    float LdotH = max(0.001, dot(L, H));
    float VdotH = max(0.001, dot(V, H));
    float HdotV = max(0.001, dot(H, V));
	float LdotX = dot(L, X);
	float LdotY = dot(L, Y);
	float VdotX = dot(V, X);
	float VdotY = dot(V, Y);
	float HdotX = dot(H, X);
	float HdotY = dot(H, Y);

#include "../include/inl/shadowing.inl.glsl"

    vec4 specularCubemap = vec4(0.0);
    float roughnessMix = clamp(1.0 - exp(-(roughness / 1.0 * log(100.0))), 0.0, 1.0);

    float perceptualRoughness = sqrt(roughness);
    float lod = 12.0 * perceptualRoughness * (2.0 - perceptualRoughness);

#if PROBE_ENABLED
    specularCubemap = SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent);
  //specularCubemap += mix(SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent), blurredSpecularCubemap, roughnessMix);
#endif // PROBE_ENABLED

#if !PROBE_ENABLED
    specularCubemap = textureLod(env_GlobalCubemap, ReflectionVector(N, position.xyz, CameraPosition), lod);
 
#if !SPHERICAL_HARMONICS_ENABLED
    irradiance = Irradiance(N).rgb;
#endif // !SPHERICAL_HARMONICS_ENABLED
#endif


#if VCT_ENABLED
    vec4 vctSpec = VCTSpecular(position.xyz, N.xyz, CameraPosition, roughness);
    vec4 vctDiff = VCTDiffuse(position.xyz, N.xyz, CameraPosition, tangent, bitangent, roughness);
    specularCubemap.rgb = mix(specularCubemap.rgb, vctSpec.rgb, vctSpec.rgb * vctSpec.a);
    gi += vctDiff;
#endif // VCT_ENABLED


#if SSR_ENABLED
    PostProcessData ppd;
    ppd.texCoord = v_texcoord0;
    ppd.resolution = Resolution;
    ppd.viewMatrix = u_viewMatrix;
    ppd.modelMatrix = u_modelMatrix;
    ppd.projMatrix = u_projMatrix;
    vec4 ssrResult = PostProcess_SSR(ppd);
    specularCubemap = mix(specularCubemap, ssrResult, ssrResult.a);
#endif


#if SPHERICAL_HARMONICS_ENABLED
    irradiance = SampleSphericalHarmonics(N);
#endif // SPHERICAL_HARMONICS_ENABLED

	float subsurface = 0.0;
	float specular = 0.5;
	float specularTint = 0.0;
	float anisotropic = 0.0;
	float sheen = 0.0;
	float sheenTint = 0.0;
	float clearcoat = 0.0;
	float clearcoatGloss = 1.0;
	
    vec3 Cdlin = mon2lin(albedo.rgb);

    // specular
    float aspect = sqrt(1.0-anisotropic*0.9);
    float ax = max(.001, sqr(roughness)/aspect);
    float ay = max(.001, sqr(roughness)*aspect);
	
	
    vec2 AB = vec2(1.0, 1.0) - BRDFMap(NdotV, perceptualRoughness);
	
	//result = mix(Fd, ss, subsurface) * Cdlin;
	
	//irradiance = mix(irradiance, gi.rgb, gi.a);
	//irradiance *= mix(vec3(1.0), gi.rgb, gi.a);
	float aperture = 16.0;
	float shutterSpeed = 1.0/125.0;
	float sensitivity = 100.0;
	float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity);
	float exposure = 1.0f / (1.2f * pow(2.0f, ev100));

    irradiance += gi.rgb;
	
	// ibl
	
	vec3 result;
	float materialReflectance = 0.5;
	float reflectance = 0.16 * materialReflectance * materialReflectance; // dielectric reflectance
	vec3 F0 = Cdlin.rgb * metallic + (reflectance * (1.0 - metallic));
	
	vec3 specularDFG = mix(vec3(AB.x), vec3(AB.y), F0);
	vec3 radiance = specularDFG * specularCubemap.rgb;
	vec3 _Fd = Cdlin.rgb * irradiance * (1.0 - specularDFG) * ao;//todo diffuse brdf for cloth
	result = _Fd+radiance;
    
    
    /// look at
    // Chan 2018, "Material Advances in Call of Duty: WWII"
    //float aperture = inversesqrt(1.0 - visibility);
    //float microShadow = saturate(NoL * aperture);
    //return microShadow * microShadow;
	
	result *= $IBL_INTENSITY * exposure;
    
    
//#if SHADING_MODEL_CLOTH
    // Energy compensation for multiple scattering in a microfacet model
    // See "Multiple-Scattering Microfacet BSDFs with the Smith Model"
    //vec3 energyCompensation = vec3(1.0) + F0 * (1.0 / AB.y - 1.0);
//#else
    vec3 energyCompensation = vec3(1.0);
//#endif

	// surface
    
#if VOLUMETRIC_LIGHTING_ENABLED
    vec3 rayDir = position.xyz - CameraPosition;
    float rayLength = length(rayDir);
    rayDir /= rayLength;
    rayLength = min(rayLength, linearDepth(u_projMatrix, depth));
    //rayDir = normalize(rayDir);
    
    //vec3 rayDeltaStep = rayDir / ($VOLUMETRIC_LIGHTING_STEPS + 1);
    
    //vec3 rayDirNorm = normalize(rayDir);
    vec4 volumetric = VolumetricLightRaymarch(v_texcoord0, CameraPosition, rayDir, rayLength);
#endif
    

    vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
    float _D = DistributionGGX(N, H, roughness); // GTR2_aniso(NdotH, HdotX, HdotY, ax, ay)
	vec3 _Fr = (_D * cookTorranceG(NdotL, NdotV, LdotH, NdotH)) * SchlickFresnel(F0, F90, LdotH);
	// Burley 2012, "Physically-Based Shading at Disney"
    //float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    vec3 lightScatter = SchlickFresnel(vec3(1.0), F90, NdotL);
    vec3 viewScatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
    vec3 _diffuse = Cdlin.rgb * (1.0 - metallic) * (lightScatter * viewScatter * (1.0 / $PI));

	vec3 surfaceShading = _diffuse + _Fr * energyCompensation;
	surfaceShading *= env_DirectionalLight.color.rgb * (/*light.attenuation*/1.0 * NdotL * ao * shadowColor.rgb);

	surfaceShading *= exposure * env_DirectionalLight.intensity;
	result += surfaceShading;

#if VOLUMETRIC_LIGHTING_ENABLED
    result += volumetric.rgb * exposure * env_DirectionalLight.intensity;
    //result += (depth < $VOLUMETRIC_LIGHTING_FADE_END) ? mix(volumetric.rgb * exposure * env_DirectionalLight.intensity, vec3(0.0), ($VOLUMETRIC_LIGHTING_FADE_END - depth)/($VOLUMETRIC_LIGHTING_FADE_END - $VOLUMETRIC_LIGHTING_FADE_START)) : vec3(0.0);
#endif

#endif

    for (int i = 0; i < env_NumPointLights; i++) {
        result += ComputePointLight(
            env_PointLights[i],
            N,
            CameraPosition,
            position.xyz,
            albedo.rgb,
            0, // todo
            roughness,
            metallic
        );
    }
	
	result.rgb = tonemap(result.rgb);

    output0 = vec4(result.rgb, 1.0);
    //output0 = vec4(texture(DepthMap, v_texcoord0));
}