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

#include "include/gbuffer.inc"
#include "include/packing.inc"
#include "include/material.inc"
#include "include/post_fx.inc"

#include "include/scene.inc"
#include "include/brdf.inc"
#include "include/tonemap.inc"

vec2 texcoord = v_texcoord0;//vec2(v_texcoord0.x, 1.0 - v_texcoord0.y);


#define HYP_VCT_ENABLED 1
#define HYP_SSR_ENABLED 1

#if HYP_VCT_ENABLED
#include "include/vct/cone_trace.inc"
#endif

#include "include/shadows.inc"

/* Begin main shader program */

#define IBL_INTENSITY 10000.0
#define DIRECTIONAL_LIGHT_INTENSITY 100000.0
#define IRRADIANCE_MULTIPLIER 1.0
#define ROUGHNESS_LOD_MULTIPLIER 32.0
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

#if 0
#define SSR_MAX_ITERATIONS 20
#define SSR_MAX_BINARY_SEARCH_ITERATIONS 5
#define SSR_PIXEL_STRIDE 16
#define SSR_PIXEL_STRIDE_CUTOFF 10
#define SSR_MAX_RAY_DISTANCE 4
#define SSR_SCREEN_EDGE_FADE_START 0.9
#define SSR_EYE_FADE_START 0.5
#define SSR_EYE_FADE_END 0.995
#define SSR_Z_THICKNESS_THRESHOLD 0.1
#define SSR_JITTER_OFFSET 0.0

float linearDepth(float depth)
{
    //return scene.camera_near * scene.camera_far / (scene.camera_far + depth * (scene.camera_near - scene.camera_far));//
    return scene.projection[3][2] / (depth * scene.projection[2][3] - scene.projection[2][2]);
}

float fetchDepth(vec2 uv)
{
    vec4 depthTexel = texture(gbuffer_depth_texture, uv);

    return depthTexel.r * 2.0 - 1.0;
}

bool rayIntersectDepth(float rayZNear, float rayZFar, vec2 hitPixel)
{
    // Swap if bigger
    if (rayZFar > rayZNear)
    {
        float t = rayZFar; rayZFar = rayZNear; rayZNear = t;
    }
    float cameraZ = linearDepth(fetchDepth(hitPixel));
    // float cameraBackZ = linearDepth(fetchDepth(backDepthTex, hitPixel));
    // Cross z
    return rayZFar <= cameraZ && rayZNear >= cameraZ - SSR_Z_THICKNESS_THRESHOLD;
}

// Trace a ray in screenspace from rayOrigin (in camera space) pointing in rayDir (in camera space)
//
// With perspective correct interpolation
//
// Returns true if the ray hits a pixel in the depth buffer
// and outputs the hitPixel (in UV space), the hitPoint (in camera space) and the number
// of iterations it took to get there.
//
// Based on Morgan McGuire & Mike Mara's GLSL implementation:
// http://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html

vec2 scene_resolution = vec2(float(scene.resolution_x), float(scene.resolution_y));

bool traceScreenSpaceRay(
    vec3 rayOrigin, vec3 rayDir, float jitter,
    out vec2 hitPixel, out vec3 hitPoint, out float iterationCount
)
{
    // Clip to the near plane
    float rayLength = ((rayOrigin.z + rayDir.z * SSR_MAX_RAY_DISTANCE) > -0.5)
        ? (-0.5 - rayOrigin.z) / rayDir.z : SSR_MAX_RAY_DISTANCE;

    vec3 rayEnd = rayOrigin + rayDir * rayLength;

    // Project into homogeneous clip space
    vec4 H0 = scene.projection * vec4(rayOrigin, 1.0);
    vec4 H1 = scene.projection * vec4(rayEnd, 1.0);

    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    // The interpolated homogeneous version of the camera space points
    vec3 Q0 = rayOrigin * k0, Q1 = rayEnd * k1;

    // Screen space endpoints
    // PENDING viewportSize ?
    vec2 P0 = (H0.xy * k0 * 0.5 + 0.5) * scene_resolution;
    vec2 P1 = (H1.xy * k1 * 0.5 + 0.5) * scene_resolution;

    // If the line is degenerate, make it cover at least one pixel to avoid handling
    // zero-pixel extent as a special case later
    P1 += dot(P1 - P0, P1 - P0) < 0.0001 ? 0.01 : 0.0;
    vec2 delta = P1 - P0;

    // Permute so that the primary iteration is in x to collapse
    // all quadrant-specific DDA case later
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        // More vertical line
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }
    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;

    // Track the derivatives of Q and K
    vec3 dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;

    vec2 dP = vec2(stepDir, delta.y * invdx);

    // Calculate pixel stride based on distance of ray origin from camera.
    // Since perspective means distant objects will be smaller in screen space
    // we can use this to have higher quality reflections for far away objects
    // while still using a large pixel stride for near objects (and increase performance)
    // this also helps mitigate artifacts on distant reflections when we use a large
    // pixel stride.
    float strideScaler = 1.0 - min(1.0, -rayOrigin.z / SSR_PIXEL_STRIDE_CUTOFF);
    float pixStride = 1.0 + strideScaler * SSR_PIXEL_STRIDE;

    // Scale derivatives by the desired pixel stride and the offset the starting values by the jitter fraction
    dP *= pixStride; dQ *= pixStride; dk *= pixStride;

    // Track ray step and derivatives in a vec4 to parallelize
    vec4 pqk = vec4(P0, Q0.z, k0);
    vec4 dPQK = vec4(dP, dQ.z, dk);

    pqk += dPQK * jitter;
    float rayZFar = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);
    float rayZNear;

    bool intersect = false;

    vec2 texelSize = 1.0 / scene_resolution;

    iterationCount = 0.0;

    for (int i = 0; i < SSR_MAX_ITERATIONS; i++)
    {
        pqk += dPQK;

        rayZNear = rayZFar;
        rayZFar = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);

        hitPixel = permute ? pqk.yx : pqk.xy;
        hitPixel *= texelSize;

        intersect = rayIntersectDepth(rayZNear, rayZFar, hitPixel);

        iterationCount += 1.0;

        // PENDING Right on all platforms?
        if (intersect) {
            break;
        }
    }

    // Binary search refinement
    // FIXME If intersect in first iteration binary search may easily lead to the pixel of reflect object it self
    if (pixStride > 1.0 && intersect && iterationCount > 1.0)
    {
        // Roll back
        pqk -= dPQK;
        dPQK /= pixStride;

        float originalStride = pixStride * 0.5;
        float stride = originalStride;

        rayZNear = pqk.z / pqk.w;
        rayZFar = rayZNear;

        for (int j = 0; j < SSR_MAX_BINARY_SEARCH_ITERATIONS; j++)
        {
            pqk += dPQK * stride;
            rayZNear = rayZFar;
            rayZFar = (dPQK.z * -0.5 + pqk.z) / (dPQK.w * -0.5 + pqk.w);
            hitPixel = permute ? pqk.yx : pqk.xy;
            hitPixel *= texelSize;

            originalStride *= 0.5;
            stride = rayIntersectDepth(rayZNear, rayZFar, hitPixel) ? -originalStride : originalStride;
        }
    }

    Q0.xy += dQ.xy * iterationCount;
    Q0.z = pqk.z;
    hitPoint = Q0 / pqk.w;

    return intersect;
}

float calculateAlpha(
    float iterationCount, float reflectivity,
    vec2 hitPixel, vec3 hitPoint, float dist, vec3 rayDir
)
{
    float alpha = 1.0;
    // Fade ray hits that approach the maximum iterations
    alpha *= 1.0 - (iterationCount / float(SSR_MAX_ITERATIONS));
    // Fade ray hits that approach the screen edge
    vec2 hitPixelNDC = hitPixel * 2.0 - 1.0;
    float maxDimension = min(1.0, max(abs(hitPixelNDC.x), abs(hitPixelNDC.y)));
    alpha *= 1.0 - max(0.0, maxDimension - SSR_SCREEN_EDGE_FADE_START) / (1.0 - SSR_SCREEN_EDGE_FADE_START);

    // Fade ray hits base on how much they face the camera
    //float eyeDir = clamp(rayDir.z, $SSR_EYE_FADE_START, $SSR_EYE_FADE_END);
    //alpha *= 1.0 - (eyeDir - $SSR_EYE_FADE_START) / ($SSR_EYE_FADE_END - $SSR_EYE_FADE_START);

    // Fade ray hits based on distance from ray origin
    alpha *= 1.0 - clamp(dist / SSR_MAX_RAY_DISTANCE, 0.0, 1.0);

    return alpha;
}

vec4 ScreenSpaceReflection(float roughness)
{
	vec3 N = DecodeNormal(texture(gbuffer_normals_texture, texcoord));
    vec3 viewSpaceN = normalize((scene.view * vec4(N, 0.0)).xyz);

    // Position in view
    vec4 projectedPos = vec4(texcoord * 2.0 - 1.0, fetchDepth(texcoord), 1.0);
    vec4 pos = inverse(scene.projection) * projectedPos;
    vec3 rayOrigin = pos.xyz / pos.w;

    vec3 rayDir = normalize(reflect(normalize(rayOrigin), viewSpaceN));


	vec2 hitPixel;
    vec3 hitPoint;
    float iterationCount;

    vec2 uv2 = texcoord * scene_resolution;
    float jitter = fract((texcoord.x + texcoord.y) * 0.25 + SSR_JITTER_OFFSET);

    bool intersect = traceScreenSpaceRay(rayOrigin, rayDir, jitter, hitPixel, hitPoint, iterationCount);

    float dist = distance(rayOrigin, hitPoint);

    float alpha = calculateAlpha(iterationCount, 1.0 /* reflectivity */, hitPixel, hitPoint, dist, rayDir) * float(intersect);

    vec3 hitNormal = DecodeNormal(texture(gbuffer_normals_texture, hitPixel));
    hitNormal = normalize((scene.view * vec4(hitNormal, 0.0)).xyz);


    if (dot(hitNormal, rayDir) >= 0.0) {
        return vec4(0.0);
    }
    if (!intersect) {
        return vec4(0.0);
    }

	return textureLod(gbuffer_mip_chain, hitPixel, mix(0.0, 9.0, roughness)) * alpha;
}


// END SSR
#else

const float rayStep = 0.25f;
const int iterationCount = 100;
const float distanceBias = 0.02f;
const float offset = 0.01f;
const int sampleCount = 4;
const bool isSamplingEnabled = false;
const bool isExponentialStepEnabled = false;
const bool isAdaptiveStepEnabled = true;
const bool isBinarySearchEnabled = false;
const bool debugDraw = false;
const float samplingCoefficient = 0.3;
const float eyeFadeStart = 0.8;
const float eyeFadeEnd = 0.9;
const float maxRayDistance = 32.0;
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

	float dist = distance(origin, hitPosition);
	// Fade ray hits based on distance from ray origin
	alpha *= 1.0 - clamp(dist / maxRayDistance, 0.0, 1.0);

	return alpha;
}

vec4 ConeTraceSSR(vec2 raySS, vec2 positionSS, float roughness)
{
    float gloss = 1.0 - 0.0;// - specularAll.a;
    float specularPower = 400.0;//roughnessToSpecularPower(0.5);

    // convert to cone angle (maximum extent of the specular lobe aperture)
    // only want half the full cone angle since we're slicing the isosceles triangle in half to get a right triangle
    float coneTheta = 0.001; //specularPowerToConeAngle(specularPower) * 0.5f;

    // P1 = positionSS, P2 = raySS, adjacent length = ||P2 - P1||
    vec2 deltaP = raySS.xy - positionSS.xy;
    float adjacentLength = length(deltaP);
    vec2 adjacentUnit = normalize(deltaP);

    vec4 totalColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
    float remainingAlpha = 1.0f;
    float maxMipLevel = 9.0/*num mips */ - 1.0f;
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
        vec4 newColor = coneSampleWeightedColor(samplePos, mipChannel, glossMult);

        remainingAlpha -= newColor.a;
        if(remainingAlpha < 0.0f)
        {
            newColor.rgb *= (1.0f - abs(remainingAlpha));
        }
        totalColor += newColor;

        if(totalColor.a >= 1.0f)
        {
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

		depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(gbuffer_depth_texture, screenPosition).x).z);
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


            return ConeTraceSSR(screenPosition, generateProjectedPosition(position), 0.0);
            //return vec4(textureLod(gbuffer_mip_chain, screenPosition, 0.0).xyz * alpha, alpha);
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
    float depth = texture(gbuffer_depth_texture, texcoord).r;
    vec4 position = vec4(generatePositionFromDepth(texcoord, depth), 1.0);
	vec4 normal = scene.view * vec4(DecodeNormal(texture(gbuffer_normals_texture, texcoord)), 0.0);

	if (roughness > 0.99) {
		return vec4(0.0);
	}

	vec3 reflectionDirection = normalize(reflect(position.xyz, normalize(normal.xyz)));
	
	return SSR(position.xyz, normalize(reflectionDirection + (normal.xyz * offset) ), roughness);
    //return ScreenSpaceConeTracing(0.15, position.xyz, reflectionDirection, 0.025f, 20.0);
	//}
}





#endif


void main()
{
    vec4 albedo   = texture(gbuffer_albedo_texture, texcoord);
    vec4 normal   = vec4(DecodeNormal(texture(gbuffer_normals_texture, texcoord)), 1.0);
    vec4 tangent   = vec4(DecodeNormal(texture(gbuffer_tangents_texture, texcoord)), 1.0);
    vec4 bitangent   = vec4(DecodeNormal(texture(gbuffer_bitangents_texture, texcoord)), 1.0);
    vec4 position = texture(gbuffer_positions_texture, texcoord);
    vec4 material = texture(gbuffer_material_texture, texcoord); /* r = roughness, g = metalness, b = ?, a = AO */
    
    bool perform_lighting = albedo.a > 0.0;
    
    vec3 albedo_linear = albedo.rgb;
	vec3 result = vec3(0.0);

    /* Physical camera */
    float aperture = 16.0;
    float shutter = 1.0/125.0;
    float sensitivity = 100.0;
    float ev100 = log2((aperture * aperture) / shutter * 100.0f / sensitivity);
    float exposure = 1.0f / (1.2f * pow(2.0f, ev100));
    
    vec3 N = normal.xyz;
    vec3 T = tangent.xyz;
    vec3 B = bitangent.xyz;
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
        vec4 screen_space_reflections = ScreenSpaceReflection2(material.r);//ScreenSpaceReflection(material.r);
        reflections = mix(reflections, screen_space_reflections, screen_space_reflections.a);
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
        //result = (irradiance * IRRADIANCE_MULTIPLIER) * (exposure * IBL_INTENSITY);

        //result = screen_space_reflections.rgb;

    } else {
        result = albedo.rgb;
    }

#if SSAO_DEBUG
    result = vec3(ao);
#endif

    output_color = vec4(Tonemap(result), 1.0);

   // output_color.rgb = result;

}
