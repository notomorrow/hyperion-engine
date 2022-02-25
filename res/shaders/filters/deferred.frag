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

uniform vec2 Resolution;

uniform vec3 CameraPosition;
uniform vec2 CameraNearFar;
uniform mat4 InverseViewProjMatrix;

float linearDepth(mat4 projMatrix, float depth)
{
    return projMatrix[3][2] / (depth * projMatrix[2][3] - projMatrix[2][2]);
}

#define $VOLUMETRIC_LIGHTING_ENABLED 1

#include "../post/modules/base.glsl"
#if SSR_ENABLED
#include "../post/modules/ssr.glsl"
#endif

#if VOLUMETRIC_LIGHTING_ENABLED
#include "../post/modules/light_rays.glsl"
#endif

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



/**
 * Approximates acos(x) with a max absolute error of 9.0x10^-3.
 * Valid in the range -1..1.
 */
float acosFast(float x) {
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
float acosFastPositive(float x) {
    float p = -0.1565827 * x + 1.570796;
    return p * sqrt(1.0 - x);
}
float sphericalCapsIntersection(float cosCap1, float cosCap2, float cosDistance) {
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
// This function could (should?) be implemented as a 3D LUT instead, but we need to save samplers
float SpecularAO_Cones(vec3 bentNormal, float visibility, float roughness, vec3 shading_reflected) {
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
float SpecularAO_Lagarde(float NoV, float visibility, float roughness) {
    // Lagarde and de Rousiers 2014, "Moving Frostbite to PBR"
    return clamp(pow(NoV + visibility, exp2(-16.0 * roughness - 1.0)) - 1.0 + visibility, 0.0, 1.0);
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
    specularCubemap = SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, lod);
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


	float aperture = 16.0;
	float shutterSpeed = 1.0/125.0;
	float sensitivity = 100.0;
	float ev100 = log2((aperture * aperture) / shutterSpeed * 100.0f / sensitivity);
	float exposure = 1.0f / (1.2f * pow(2.0f, ev100));

    PostProcessData ppd;
    ppd.texCoord = v_texcoord0;
    ppd.resolution = Resolution;
    ppd.worldPosition = position.xyz;
    ppd.cameraPosition = CameraPosition;
    ppd.depth = depth;
    ppd.linearDepth = linearDepth(u_projMatrix, depth);
    ppd.exposure = exposure;
    ppd.viewMatrix = u_viewMatrix;
    ppd.modelMatrix = u_modelMatrix;
    ppd.projMatrix = u_projMatrix;
    
    PostProcessResult ppr;
    ppr.color = vec4(0.0);
    ppr.mode = 0;//$POST_PROCESS_EQL;

#if SSR_ENABLED
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
    vec3 energyCompensation = vec3(1.0);

    // specular
    float aspect = sqrt(1.0-anisotropic*0.9);
    float ax = max(.001, sqr(roughness)/aspect);
    float ay = max(.001, sqr(roughness)*aspect);
	
    vec2 AB = vec2(1.0, 1.0) - BRDFMap(NdotV, perceptualRoughness);

    irradiance += gi.rgb * $GI_INTENSITY;
	
	// ibl
	vec3 result;
	float materialReflectance = 0.5;
	float reflectance = 0.16 * materialReflectance * materialReflectance; // dielectric reflectance
	vec3 F0 = Cdlin.rgb * metallic + (reflectance * (1.0 - metallic));
    
    //vec3 visibility = shadowColor.rgb * ao;
	
	vec3 specularDFG = mix(vec3(AB.x), vec3(AB.y), F0);
    vec3 specularSingleBounceAO = vec3(SpecularAO_Cones(N, ao, roughness, ReflectionVector(N, position.xyz, CameraPosition)));//SpecularAO_Lagarde(NdotV, ao, roughness) * energyCompensation;//vec3(ao) * NdotV * pixel.energyCompensation;
	vec3 radiance = specularDFG * specularCubemap.rgb * specularSingleBounceAO;
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
//#endif

#if VOLUMETRIC_LIGHTING_ENABLED
    PostProcess_LightRays(ppd, ppr);
    result = (ppr.mode == $POST_PROCESS_EQL) ? ppr.color.rgb :
        (ppr.mode == $POST_PROCESS_ADD) ? result + ppr.color.rgb :
        (ppr.mode == $POST_PROCESS_MUL) ? result * ppr.color.rgb : result;
#endif

	// surface
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
}