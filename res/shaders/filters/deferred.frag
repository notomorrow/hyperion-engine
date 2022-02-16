#version 330 core

#define $PI 3.141592654

#include "../include/matrices.inc"
#include "../include/frag_output.inc"
#include "../include/depth.inc"
#include "../include/lighting.inc"
#include "../include/material.inc"
#include "../include/parallax.inc"
#include "../include/tonemap.inc"

#if SHADOWS
#include "../include/shadows.inc"
#endif

in vec2 v_texcoord0;
uniform sampler2D ColorMap;
uniform sampler2D DepthMap;
uniform sampler2D PositionMap;
//uniform sampler2D NormalMap; // included in lighting.inc
uniform sampler2D DataMap;
uniform sampler2D TangentMap;
uniform sampler2D BitangentMap;

uniform sampler3D LightVolumeMap;

uniform sampler2D SSLightingMap;
uniform int HasSSLightingMap;

uniform vec3 CameraPosition;
uniform mat4 InverseViewProjMatrix;

#include "../post/modules/base.glsl"
#include "../post/modules/ssr.glsl"

#define $DIRECTIONAL_LIGHTING 1

#define $BLAH 0

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




#define $SSR_ENABLED 1

#if SSR_ENABLED



uniform float rayStep = 0.1f;
uniform int iterationCount = 100;
uniform float distanceBias = 0.02f;
uniform float offset = 0.1f;
uniform bool enableSSR = false;
uniform int sampleCount = 4;
uniform bool isSamplingEnabled = false;
uniform bool isExponentialStepEnabled = false;
uniform bool isAdaptiveStepEnabled = false;
uniform bool isBinarySearchEnabled = true;
uniform bool debugDraw = false;
uniform float samplingCoefficient = 0.3;

vec3 generatePositionFromDepth(vec2 texturePos, float depth) {
	vec4 ndc = vec4((texturePos - 0.5) * 2, depth, 1.f);
	vec4 inversed = inverse(u_projMatrix) * ndc;// going back from projected
	inversed /= inversed.w;
	return inversed.xyz;
}

vec2 generateProjectedPosition(vec3 pos){
	vec4 samplePosition = u_projMatrix * vec4(pos, 1.f);
	samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
	return samplePosition.xy;
}

float SSRAlpha(vec3 dir, vec3 origin, vec3 hitPosition)
{
	float alpha = 1.0;

	float dist = distance(origin, hitPosition);
	// Fade ray hits based on distance from ray origin
	alpha *= 1.0 - clamp(dist / 6, 0.0, 1.0);
	
	return alpha;
}

vec3 SSR(vec3 position, vec3 reflection) {
	vec3 step = rayStep * reflection;
	vec3 marchingPosition = position + step;
	float delta;
	float depthFromScreen;
	vec2 screenPosition;
	
	int i = 0;
	for (; i < iterationCount; i++) {
		screenPosition = generateProjectedPosition(marchingPosition);
		depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(DepthMap, screenPosition).x).z);
		delta = abs(marchingPosition.z) - depthFromScreen;
		if (abs(delta) < distanceBias) {
			return texture(ColorMap, screenPosition).xyz * SSRAlpha(reflection, position, marchingPosition);
		}
		if (isBinarySearchEnabled && delta > 0) {
			break;
		}
		if (isAdaptiveStepEnabled){
			float directionSign = sign(abs(marchingPosition.z) - depthFromScreen);
			//this is sort of adapting step, should prevent lining reflection by doing sort of iterative converging
			//some implementation doing it by binary search, but I found this idea more cheaty and way easier to implement
			step = step * (1.0 - rayStep * max(directionSign, 0.0));
			marchingPosition += step * (-directionSign);
		}
		else {
			marchingPosition += step;
		}
		if (isExponentialStepEnabled){
			step *= 1.05;
		}
    }
	if(isBinarySearchEnabled){
		for(; i < iterationCount; i++){
			
			step *= 0.5;
			marchingPosition = marchingPosition - step * sign(delta);
			
			screenPosition = generateProjectedPosition(marchingPosition);
			depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(DepthMap, screenPosition).x).z);
			delta = abs(marchingPosition.z) - depthFromScreen;
			
			if (abs(delta) < distanceBias) {
				return texture(ColorMap, screenPosition).xyz * SSRAlpha(reflection, position, marchingPosition);
			}
		}
	}
	
    return vec3(0.0);
}

#if BLAH
void main(){
	vec2 UV = v_texcoord0 / Resolution;
    float depth = texture(DepthMap, v_texcoord0).r;
    vec4 position = vec4(generatePositionFromDepth(v_texcoord0, depth), 1.0);
	vec4 normal = u_viewMatrix * vec4(texture(NormalMap, v_texcoord0).xyz * 2.0 - 1.0, 0.0);
    vec4 data = texture(DataMap, v_texcoord0);
    float roughness = clamp(data.g, 0.05, 0.99);

	if (roughness < 0.01) {
		output0 = texture(ColorMap, v_texcoord0);
	} else {
		vec3 reflectionDirection = normalize(reflect(position.xyz, normalize(normal.xyz)));
		if (isSamplingEnabled) {
		vec3 firstBasis = normalize(cross(normalize(u_viewMatrix * vec4(0.f, 0.f, 1.f, 1.0)).xyz, reflectionDirection));
		vec3 secondBasis = normalize(cross(reflectionDirection, firstBasis));
		vec4 resultingColor = vec4(0.f);
		for (int i = 0; i < sampleCount; i++) {
			vec2 coeffs = vec2(random(v_texcoord0 + vec2(0, i) / Resolution) + random(v_texcoord0 + vec2(i, 0) / Resolution)) * samplingCoefficient;
			vec3 reflectionDirectionRandomized = reflectionDirection + firstBasis * coeffs.x + secondBasis * coeffs.y;
			vec3 tempColor = SSR(position.xyz, normalize(reflectionDirectionRandomized));
			if (tempColor != vec3(0.f)) {
				resultingColor += vec4(tempColor, 1.f);
			}
		}
		if (resultingColor.w == 0){
			output0 = texture(ColorMap, v_texcoord0);
		} else {
			resultingColor /= resultingColor.w;
			output0 = vec4(resultingColor.xyz, 1.f);
		}
		} else {
			output0 = vec4(SSR(position.xyz, normalize(reflectionDirection ) + (normal.xyz)), 1.f);
		}
	}
}
#endif



#endif
#if !BLAH

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

#if SHADOWS
    float shadowness = 0.0;
    int shadowSplit = getShadowMapSplit(distance(CameraPosition, position.xyz));

#if SHADOW_PCF
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            vec2 offset = poissonDisk[x * 4 + y] * $SHADOW_MAP_RADIUS;
            vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz + vec3(offset.x, offset.y, -offset.x));
            shadowness += getShadow(shadowSplit, shadowCoord, NdotL);
        }
    }

    shadowness /= 16.0;
#endif // !SHADOW_PCF

#if !SHADOW_PCF
    vec3 shadowCoord = getShadowCoord(shadowSplit, position.xyz);
    shadowness = getShadow(shadowSplit, shadowCoord, NdotL);
#endif // !SHADOW_PCF

    vec4 shadowColor = vec4(vec3(shadowness), 1.0);
    shadowColor = CalculateFogLinear(shadowColor, vec4(1.0), position.xyz, CameraPosition, u_shadowSplit[int($NUM_SPLITS) - 2], u_shadowSplit[int($NUM_SPLITS) - 1]);
#endif // SHADOWS

#if !SHADOWS
    float shadowness = 1.0;
    vec4 shadowColor = vec4(1.0);
#endif 

  vec4 specularCubemap = vec4(0.0);
  float roughnessMix = clamp(1.0 - exp(-(roughness / 1.0 * log(100.0))), 0.0, 1.0);


#if PROBE_ENABLED
  specularCubemap = SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent);
  //specularCubemap += mix(SampleEnvProbe(env_GlobalCubemap, N, position.xyz, CameraPosition, tangent, bitangent), blurredSpecularCubemap, roughnessMix);
#endif // PROBE_ENABLED

#if !PROBE_ENABLED
  float perceptualRoughness = sqrt(roughness);
  float lod = 12.0 * perceptualRoughness * (2.0 - perceptualRoughness);
  specularCubemap = textureLod(env_GlobalCubemap, ReflectionVector(N, position.xyz, CameraPosition), lod);
 
#if !SPHERICAL_HARMONICS_ENABLED
  irradiance = EnvRemap(Irradiance(N)).rgb;
#endif // !SPHERICAL_HARMONICS_ENABLED
#endif


#if VCT_ENABLED
  vec4 vctSpec = VCTSpecular(position.xyz, N.xyz, CameraPosition, roughness);
  vec4 vctDiff = VCTDiffuse(position.xyz, N.xyz, CameraPosition, tangent, bitangent, roughness);
  specularCubemap = mix(specularCubemap, vctSpec, vctSpec.a);
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
  specularCubemap = ssrResult;//mix(specularCubemap, ssrResult, ssrResult.a);
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
	
#define $PBR_IBL_MATERIAL 1
#define $PBR_CLOTH_MATERIAL 1

	
    vec3 Cdlin = mon2lin(albedo.rgb);
    float Cdlum = .3*Cdlin[0] + .6*Cdlin[1]  + .1*Cdlin[2]; // luminance approx.

    vec3 Ctint = Cdlum > 0 ? Cdlin/Cdlum : vec3(1); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(specular*.08*mix(vec3(1), Ctint, specularTint) , Cdlin, metallic);
    vec3 Csheen = mix(vec3(1), Ctint, sheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnelRoughness2(NdotL, mix(vec3(0.04), vec3(1.0), metallic), roughness).r,  //FresnelTerm( mix(vec3(0.04), vec3(1.0), metallic), NdotL).r,//SchlickFresnel2(NdotL),
		FV = SchlickFresnelRoughness2(NdotV, mix(vec3(0.04), vec3(1.0), metallic), roughness).r;  //FresnelTerm(mix(vec3(0.04), vec3(1.0), metallic), NdotV).r;//SchlickFresnel2(NdotV);
    float Fd90 = 0.5 + 2 * LdotH*LdotH * roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH*LdotH*roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

    // specular
    float aspect = sqrt(1.0-anisotropic*0.9);
    float ax = max(.001, sqr(roughness)/aspect);
    float ay = max(.001, sqr(roughness)*aspect);
    float Ds = clamp(GTR2_aniso(NdotH, HdotX, HdotY, ax, ay), 0.0, 1.0);
    float FH =   SchlickFresnelRoughness2(LdotH, mix(vec3(0.04), vec3(1.0), metallic), roughness).r; ///FresnelTerm(mix(vec3(0.04), vec3(1.0), metallic), LdotH).r; //SchlickFresnel2(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1), FH);
    float Gs;
    //Gs  = smithG_GGX_aniso(NdotL, LdotX, LdotY, ax, ay);
    //Gs *= smithG_GGX_aniso(NdotV, VdotX, VdotY, ax, ay);
	
	Gs = cookTorranceG(NdotL, NdotV, LdotH, NdotH);//SmithGGXSchlickVisibility(clamp(NdotL, 0.0, 1.0), clamp(NdotV, 0.0, 1.0), roughness);

    // sheen
    vec3 Fsheen = FH * sheen * Csheen;

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(.1,.001,clearcoatGloss));
    float Fr = mix(.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);
	
	
    vec2 AB = BRDFMap(NdotV, roughness);
	
	//result = mix(Fd, ss, subsurface) * Cdlin;
	float reflectance = 1.0;
	
	vec3 result;
	vec3 F0 = Cdlin.rgb * metallic + (reflectance * (1.0 - metallic));
	
	vec3 specularDFG = mix(vec3(AB.x), vec3(AB.y), F0);
	vec3 radiance = specularDFG * specularCubemap.rgb;
	vec3 _Fd = Cdlin.rgb * mix(irradiance.rgb, gi.rgb, gi.a) * (1.0 - specularDFG) * ao;//todo diffuse brdf for cloth
	result = _Fd+radiance;

	vec3 _Fr = (GTR2_aniso(NdotH, HdotX, HdotY, ax, ay) * cookTorranceG(NdotL, NdotV, LdotH, NdotH)) * SchlickFresnelRoughness(F0, LdotH);
	// Burley 2012, "Physically-Based Shading at Disney"
    float f90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float lightScatter = SchlickFresnel(1.0, f90, NdotL);
    float viewScatter  = SchlickFresnel(1.0, f90, NdotV);
    vec3 _diffuse = Cdlin.rgb * (1.0 - metallic) * (lightScatter * viewScatter * (1.0 / $PI));
	result = result + _diffuse*ao;

#endif

    /*for (int i = 0; i < env_NumPointLights; i++) {
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
    }*/
	
	result.rgb = tonemap(result.rgb);

    output0 = vec4(result, 1.0);
}
#endif