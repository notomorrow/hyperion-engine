#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 output_color;
layout(location=1) out vec4 output_normals;
layout(location=2) out vec4 output_positions;

// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 12) uniform texture2D ssr_uvs;
// layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 13) uniform texture2D ssr_sample;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 21) uniform texture2D ssr_blur_vert;

#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/post_fx.inc"
#include "include/tonemap.inc"

#include "include/scene.inc"
#include "include/brdf.inc"

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);


#define HYP_VCT_ENABLED 0
#define HYP_SSR_ENABLED 0

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

#include "include/shadows.inc"

/* Begin main shader program */

#define IBL_INTENSITY 30000.0
#define DIRECTIONAL_LIGHT_INTENSITY 100000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define ROUGHNESS_LOD_MULTIPLIER 12.0
#define SSAO_DEBUG 0

#include "include/rt/probe/shared.inc"

#if 0

vec4 SampleIrradiance(vec3 P, vec3 N, vec3 V)
{
    const uvec3 base_grid_coord    = BaseGridCoord(P);
    const vec3 base_probe_position = GridPositionToWorldPosition(base_grid_coord);
    
    vec3 total_irradiance = vec3(0.0);
    float total_weight    = 0.0;
    
    vec3 alpha = clamp((P - base_probe_position) / PROBE_GRID_STEP, vec3(0.0), vec3(1.0));
    
    for (uint i = 0; i < 8; i++) {
        uvec3 offset = uvec3(i, i >> 1, i >> 2) & uvec3(1);
        uvec3 probe_grid_coord = clamp(base_grid_coord + offset, uvec3(0.0), probe_system.probe_counts.xyz - uvec3(1));
        
        uint probe_index    = GridPositionToProbeIndex(probe_grid_coord);
        vec3 probe_position = GridPositionToWorldPosition(probe_grid_coord);
        vec3 probe_to_point = P - probe_position + (N + 3.0 * V) * PROBE_NORMAL_BIAS;
        vec3 dir            = normalize(-probe_to_point);
        
        vec3 trilinear = mix(vec3(1.0) - alpha, alpha, vec3(offset));
        float weight   = 1.0;
        
        /* Backface test */
        
        vec3 true_direction_to_probe = normalize(probe_position - P);
        weight *= SQR(max(0.0001, (dot(true_direction_to_probe, N) + 1.0) * 0.5)) + 0.2;
        
        /* Visibility test */
        /* TODO */
        
        weight = max(0.000001, weight);
        
        vec3 irradiance_direction = N;
        
    }
}

#endif

// BEGIN SSR

#if HYP_SSR_ENABLED

const float rayStep = 0.45;
const int iterationCount = 100;
const float distanceBias = 0.02f;
const float offset = 0.01f;
const int sampleCount = 4;
const bool isSamplingEnabled = false;
const bool isExponentialStepEnabled = false;
const bool isAdaptiveStepEnabled = true;
const bool isBinarySearchEnabled = false;
const bool debugDraw = false;
const float eyeFadeStart = 0.2;
const float eyeFadeEnd = 0.95;
const float maxRayDistance = 64.0;
const float ssr_screen_edge_fade_start = 0.7;
const float ssr_screen_edge_fade_end = 0.9;


///////////////////////////////////////////////////////////////////////////////////////
// Cone tracing methods
///////////////////////////////////////////////////////////////////////////////////////

float specularPowerToConeAngle(float specularPower)
{
    const float xi = 0.244;
    float exponent = 1.0 / (specularPower + 1.0);
    return acos(pow(xi, exponent));
}

float isoscelesTriangleOpposite(float adjacentLength, float coneTheta)
{
    // simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj, opp = tan(theta) * adj, then multiply * 2.0f for isosceles triangle base
    return 2.0 * tan(coneTheta) * adjacentLength;
}

float isoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0 * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}

vec4 coneSampleWeightedColor(vec2 samplePos, float mipChannel, float gloss)
{
    vec4 sampleColor = textureLod(gbuffer_mip_chain, samplePos, mipChannel);//colorBuffer.SampleLevel(sampTrilinearClamp, samplePos, mipChannel).rgb;
    return sampleColor * gloss;
}

float isoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius)
{
    // subtract the diameter of the incircle to get the adjacent side of the next level on the cone
    return adjacentLength - (incircleRadius * 2.0);
}

///////////////////////////////////////////////////////////////////////////////////////




vec3 generatePositionFromDepth(vec2 texturePos, float depth) {
	vec4 ndc = vec4((texturePos - 0.5) * 2, depth, 1.f);
	vec4 inversed = inverse(scene.projection) * ndc;// going back from projected
	inversed /= inversed.w;
	return inversed.xyz;
}

vec2 generateProjectedPosition(vec3 pos){
	vec4 samplePosition = scene.projection * vec4(pos, 1.f);
	samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
	return samplePosition.xy;
}

float SSRAlpha(vec3 dir, vec3 origin, vec3 hitPosition)
{
	float alpha = 1.0;
	// Fade ray hits base on how much they face the camera
	float _eyeFadeStart = eyeFadeStart;
	float _eyeFadeEnd = eyeFadeEnd;
	if (_eyeFadeStart > _eyeFadeEnd) {
		float tmp = _eyeFadeEnd;
		_eyeFadeEnd = _eyeFadeStart;
		_eyeFadeStart = tmp;
	}

    float eyeDirection = clamp( dir.z, eyeFadeStart, eyeFadeEnd);
    alpha *= ((eyeDirection - eyeFadeStart) / (eyeFadeEnd - eyeFadeStart));

	float dist = distance(origin, hitPosition);
	// Fade ray hits based on distance from ray origin
	alpha *= 1.0 - clamp(dist / maxRayDistance, 0.0, 1.0);

	return alpha;
}

vec4 ConeTraceSSR(vec2 raySS, vec2 positionSS, float roughness)
{
    float gloss = HYP_FMATH_SQR(1.0 - roughness);

    // convert to cone angle (maximum extent of the specular lobe aperture)
    // only want half the full cone angle since we're slicing the isosceles triangle in half to get a right triangle
    float coneTheta = 0.01;// * 0.5;

    // P1 = positionSS, P2 = raySS, adjacent length = ||P2 - P1||
    vec2 deltaP = raySS.xy - positionSS.xy;
    float adjacentLength = length(deltaP);
    vec2 adjacentUnit = normalize(deltaP);

    vec4 totalColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    float remainingAlpha = 1.0f;
    float glossMult = gloss;
    // cone-tracing using an isosceles triangle to approximate a cone in screen space
    for(int i = 0; i < 14; ++i)
    {
        // intersection length is the adjacent side, get the opposite side using trig
        float oppositeLength = isoscelesTriangleOpposite(adjacentLength, coneTheta);

        // calculate in-radius of the isosceles triangle
        float incircleSize = isoscelesTriangleInRadius(oppositeLength, adjacentLength);

        // get the sample position in screen space
        vec2 samplePos = positionSS.xy + adjacentUnit * (adjacentLength - incircleSize);

        // convert the in-radius into screen size then check what power N to raise 2 to reach it - that power N becomes mip level to sample from
        float mipChannel = log2(incircleSize * max(float(scene.resolution_x), float(scene.resolution_y)));//clamp(log2(incircleSize * max(cb_depthBufferSize.x, cb_depthBufferSize.y)), 0.0f, maxMipLevel);

        /*
         * Read color and accumulate it using trilinear filtering and weight it.
         * Uses pre-convolved image (color buffer) and glossiness to weigh color contributions.
         * Visibility is accumulated in the alpha channel. Break if visibility is 100% or greater (>= 1.0f).
         */
        vec4 newColor = coneSampleWeightedColor(samplePos, mipChannel, gloss);

        remainingAlpha -= newColor.a;
        if (remainingAlpha < 0.0f) {
            newColor.rgb *= (1.0f - abs(remainingAlpha));
        }

        totalColor += newColor;

        if(totalColor.a >= 1.0f) {
            break;
        }

        adjacentLength = isoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
        glossMult *= gloss;
    }

    return totalColor;
}

vec4 SSR(vec3 position, vec3 reflection, float roughness) {
	vec3 step = rayStep * reflection;
	vec3 marchingPosition = position + step;
	float delta;
	float depthFromScreen;
	vec2 screenPosition;
	
	int i = 0;
	for (; i < iterationCount && distance(marchingPosition, position) < maxRayDistance; i++) {
		screenPosition = generateProjectedPosition(marchingPosition);

		depthFromScreen = abs(generatePositionFromDepth(screenPosition, SampleGBuffer(gbuffer_depth_texture, screenPosition).x).z);
		delta = abs(marchingPosition.z) - depthFromScreen;

        //if (depthFromScreen >= maxRayDistance) {
        //    return vec4(0.0);
        //}

		if (abs(delta) < distanceBias) {
            float alpha = SSRAlpha(reflection, position, marchingPosition);

            vec2 hitPixelNDC = screenPosition * 2.0 - 1.0;
            float maxDimension = min(1.0, max(abs(hitPixelNDC.x), abs(hitPixelNDC.y)));
            alpha *= 1.0 - max(0.0, maxDimension - ssr_screen_edge_fade_start) / (1.0 - ssr_screen_edge_fade_start);

            alpha = clamp(alpha, 0.0, 1.0);


            // vec4 conetrace_result = ConeTraceSSR(screenPosition, generateProjectedPosition(position), roughness);

            // conetrace_result *= alpha;

            // return conetrace_result;
            return vec4(textureLod(gbuffer_mip_chain, screenPosition, 0.0));
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

    return vec4(0.0);
}


/*vec4 ScreenSpaceConeTracing(float min_diameter, vec3 origin, vec3 dir, float ratio, float max_dist)
{
	vec3 step = rayStep * dir;
	vec3 marchingPosition = origin + step;
	float delta;
	float depthFromScreen;
	vec2 screenPosition;

    const float min_diameter_inv = 1.0 / min_diameter;

    vec4 accum      = vec4(0.0);
    vec4 fade       = vec4(0.0);
    vec3 sample_pos = origin;
    float dist      = min_diameter;

    const float max_lod = 6;

    while (dist < max_dist && accum.a < 1.0) {
        float diameter = max(min_diameter, ratio * dist);
        float lod      = log2(diameter * min_diameter_inv);

        sample_pos = origin + dir * dist;

        //vec4 voxel_color = FetchVoxel(sample_pos, lod);
        //voxel_color *= 1.0 - clamp(dist / max_dist, 0.0, 1.0);
        vec4 voxel_color = vec4(0.0);

        screenPosition = generateProjectedPosition(sample_pos.xyz);
		depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(gbuffer_depth_texture, screenPosition).x).z);
		delta = abs(marchingPosition.z) - depthFromScreen;
		if (abs(delta) < distanceBias) {
            float alpha = SSRAlpha(dir, origin, sample_pos);

            vec2 hitPixelNDC = screenPosition * 2.0 - 1.0;
            float maxDimension = min(1.0, max(abs(hitPixelNDC.x), abs(hitPixelNDC.y)));
            alpha *= 1.0 - max(0.0, maxDimension - ssr_screen_edge_fade_start) / (1.0 - ssr_screen_edge_fade_start);

            alpha *= 1.0 - (distance(origin, marchingPosition) / max_dist);

            voxel_color = vec4(textureLod(gbuffer_mip_chain, screenPosition, lod).xyz * alpha, alpha);
		}

        float weight = (1.0 - accum.a);
        accum += voxel_color * weight;
        dist += diameter;
    }

    return accum;
}*/

vec4 ScreenSpaceReflection2(float roughness)
{
	vec2 UV = texcoord / vec2(float(scene.resolution_x), float(scene.resolution_y));
    float depth = SampleGBuffer(gbuffer_depth_texture, texcoord).r;
    vec4 position = vec4(generatePositionFromDepth(texcoord, depth), 1.0);
	vec4 normal = scene.view * vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 0.0);

	// if (roughness > 0.99) {
	// 	return vec4(0.0);
	// }

	vec3 reflectionDirection = normalize(reflect(position.xyz, normalize(normal.xyz)));
	
	return SSR(position.xyz, normalize(reflectionDirection + (normal.xyz * offset) ), roughness);
    //return ScreenSpaceConeTracing(0.15, position.xyz, reflectionDirection, 0.025f, 20.0);
	//}
}





#endif


void main()
{
    vec4 albedo    = SampleGBuffer(gbuffer_albedo_texture, texcoord);
    vec4 normal    = vec4(DecodeNormal(SampleGBuffer(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(SampleGBuffer(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent = vec4(DecodeNormal(SampleGBuffer(gbuffer_bitangents_texture, texcoord)), 1.0);
    vec4 position  = SampleGBuffer(gbuffer_positions_texture, texcoord);
    vec4 material  = SampleGBuffer(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    vec3 N = normalize(normal.xyz);
    vec3 T = normalize(tangent.xyz);
    vec3 B = normalize(bitangent.xyz);
    vec3 L = normalize(scene.light_direction.xyz);
    vec3 V = normalize(scene.camera_position.xyz - position.xyz);
    vec3 R = normalize(reflect(-V, N));
    vec3 H = normalize(L + V);
    
    float ao = 1.0;
    
    if (perform_lighting) {
        ao = SampleEffectPre(0, v_texcoord0, vec4(1.0)).r * material.a;

        const float roughness = material.r;
        const float metalness = material.g;
        
        float NdotL = max(0.0001, dot(N, L));
        float NdotV = max(0.0001, dot(N, V));
        float NdotH = max(0.0001, dot(N, H));
        float LdotH = max(0.0001, dot(L, H));
        float VdotH = max(0.0001, dot(V, H));
        float HdotV = max(0.0001, dot(H, V));

        float shadow = 1.0;

        if (scene.num_environment_shadow_maps != 0) {

#if HYP_SHADOW_PENUMBRA
            shadow = GetShadowContactHardened(0, position.xyz, NdotL);
#else
            shadow = GetShadow(0, position.xyz, vec2(0.0), NdotL);
#endif
        }

        const vec3 diffuse_color = albedo_linear * (1.0 - metalness);

        const float material_reflectance = 0.5;
        const float reflectance = 0.16 * material_reflectance * material_reflectance; // dielectric reflectance
        const vec3 F0 = albedo_linear.rgb * metalness + (reflectance * (1.0 - metalness));
        
        const vec2 AB = BRDFMap(roughness, NdotV);
        const vec3 dfg = albedo_linear.rgb * AB.x + AB.y;
        
        const vec3 energy_compensation = 1.0 + F0 * (AB.y - 1.0);
        const float perceptual_roughness = sqrt(roughness);
        const float lod = ROUGHNESS_LOD_MULTIPLIER * perceptual_roughness * (2.0 - perceptual_roughness);
        
        vec3 irradiance = vec3(0.0);
        vec4 reflections = vec4(0.0);

#if HYP_FEATURES_BINDLESS_TEXTURES // for now, unitl fixed
        vec3 ibl = HasEnvironmentTexture(0)
            ? textureLod(cubemap_textures[scene.environment_texture_index], R, lod).rgb
            : vec3(0.0);
#else
        vec3 ibl = vec3(1.0);
#endif

#if HYP_VCT_ENABLED
        vec4 vct_specular = ConeTraceSpecular(position.xyz, N, R, roughness);
        vec4 vct_diffuse  = ConeTraceDiffuse(position.xyz, N, T, B, roughness);

        irradiance  = vct_diffuse.rgb;
        reflections = vct_specular;
#endif

#if HYP_SSR_ENABLED
        // vec4 screen_space_reflections = Texture2D(gbuffer_sampler, ssr_sample, texcoord);
        // reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a);
#endif

        vec3 light_color = vec3(1.0);
        
        // ibl
        //vec3 dfg =  AB;// mix(vec3(AB.x), vec3(AB.y), vec3(1.0) - F0);
        vec3 specular_ao = vec3(SpecularAO_Lagarde(NdotV, ao, roughness));
        vec3 radiance = dfg * ibl * specular_ao;
        result = (diffuse_color * (irradiance * IRRADIANCE_MULTIPLIER) * (1.0 - dfg) * ao) + radiance;
        result *= exposure * IBL_INTENSITY;

        //reflections.rgb *= textureLod(gbuffer_mip_chain, texcoord, 2).rgb;
        result = (result * (1.0 - reflections.a)) + (dfg * reflections.rgb);

        // surface
        vec3 F90 = vec3(clamp(dot(F0, vec3(50.0 * 0.33)), 0.0, 1.0));
        float D = DistributionGGX(N, H, roughness);
        float G = CookTorranceG(NdotL, NdotV, LdotH, NdotH);
        vec3 F = SchlickFresnel(F0, F90, LdotH);
        vec3 specular = D * G * F;
        //vec3 fresnel = (F * G * D) / (4.0 * NdotL * NdotV);
        
        vec3 light_scatter = SchlickFresnel(vec3(1.0), F90, NdotL);
        vec3 view_scatter  = SchlickFresnel(vec3(1.0), F90, NdotV);
        vec3 diffuse = diffuse_color * (light_scatter * view_scatter * (1.0 / PI));

        // Chan 2018, "Material Advances in Call of Duty: WWII"
        //float micro_shadow_aperture = inversesqrt(1.0 - ao);
        //float micro_shadow = clamp(NdotL * micro_shadow_aperture, 0.0, 1.0);
        //float micro_shadow_sqr = micro_shadow * micro_shadow;

        result += (specular + diffuse * energy_compensation) * ((exposure * DIRECTIONAL_LIGHT_INTENSITY) * NdotL * ao * shadow);
        //result = reflections.rgb;
    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color = vec4(result, 1.0);
    output_color.rgb = Tonemap(output_color.rgb);


//    output_color.rgb = Texture2D(gbuffer_sampler, ssr_blur_vert, texcoord).rgb;

}
