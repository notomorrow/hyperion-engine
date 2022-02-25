

#define $SSR_MAX_ITERATIONS 20
#define $SSR_MAX_BINARY_SEARCH_ITERATIONS 5
#define $SSR_PIXEL_STRIDE 20
#define $SSR_PIXEL_STRIDE_CUTOFF 10
#define $SSR_MAX_RAY_DISTANCE 6
#define $SSR_SCREEN_EDGE_FADE_START 0.9
#define $SSR_EYE_FADE_START 0.7
#define $SSR_EYE_FADE_END 0.8
#define $SSR_Z_THICKNESS_THRESHOLD 0.1
#define $SSR_JITTER_OFFSET 0

uniform float CamFar;
uniform float CamNear;

float fetchDepth(vec2 uv, vec2 resolution)
{
    vec4 depthTexel = Texture2DValue(DepthMap, uv, resolution);
    return depthTexel.r * 2.0 - 1.0;
}

bool rayIntersectDepth(PostProcessData data, float rayZNear, float rayZFar, vec2 hitPixel)
{
    // Swap if bigger
    if (rayZFar > rayZNear)
    {
        float t = rayZFar; rayZFar = rayZNear; rayZNear = t;
    }
    float cameraZ = linearDepth(data.projMatrix, fetchDepth(hitPixel, data.resolution));
    // float cameraBackZ = linearDepth(fetchDepth(backDepthTex, hitPixel));
    // Cross z
    return rayZFar <= cameraZ && rayZNear >= cameraZ - $SSR_Z_THICKNESS_THRESHOLD;
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

bool traceScreenSpaceRay(
	PostProcessData data,
    vec3 rayOrigin, vec3 rayDir, float jitter,
    out vec2 hitPixel, out vec3 hitPoint, out float iterationCount
)
{
    // Clip to the near plane
    float rayLength = ((rayOrigin.z + rayDir.z * $SSR_MAX_RAY_DISTANCE) > -0.5)
        ? (-0.5 - rayOrigin.z) / rayDir.z : $SSR_MAX_RAY_DISTANCE;

    vec3 rayEnd = rayOrigin + rayDir * rayLength;

    // Project into homogeneous clip space
    vec4 H0 = u_projMatrix * vec4(rayOrigin, 1.0);
    vec4 H1 = u_projMatrix * vec4(rayEnd, 1.0);

    float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;

    // The interpolated homogeneous version of the camera space points
    vec3 Q0 = rayOrigin * k0, Q1 = rayEnd * k1;

    // Screen space endpoints
    // PENDING viewportSize ?
    vec2 P0 = (H0.xy * k0 * 0.5 + 0.5) * Resolution;
    vec2 P1 = (H1.xy * k1 * 0.5 + 0.5) * Resolution;

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
    float strideScaler = 1.0 - min(1.0, -rayOrigin.z / $SSR_PIXEL_STRIDE_CUTOFF);
    float pixStride = 1.0 + strideScaler * $SSR_PIXEL_STRIDE;

    // Scale derivatives by the desired pixel stride and the offset the starting values by the jitter fraction
    dP *= pixStride; dQ *= pixStride; dk *= pixStride;

    // Track ray step and derivatives in a vec4 to parallelize
    vec4 pqk = vec4(P0, Q0.z, k0);
    vec4 dPQK = vec4(dP, dQ.z, dk);

    pqk += dPQK * jitter;
    float rayZFar = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);
    float rayZNear;

    bool intersect = false;

    vec2 texelSize = 1.0 / Resolution;

    iterationCount = 0.0;

    for (int i = 0; i < $SSR_MAX_ITERATIONS; i++)
    {
        pqk += dPQK;

        rayZNear = rayZFar;
        rayZFar = (dPQK.z * 0.5 + pqk.z) / (dPQK.w * 0.5 + pqk.w);

        hitPixel = permute ? pqk.yx : pqk.xy;
        hitPixel *= texelSize;

        intersect = rayIntersectDepth(data, rayZNear, rayZFar, hitPixel);

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

        for (int j = 0; j < $SSR_MAX_BINARY_SEARCH_ITERATIONS; j++)
        {
            pqk += dPQK * stride;
            rayZNear = rayZFar;
            rayZFar = (dPQK.z * -0.5 + pqk.z) / (dPQK.w * -0.5 + pqk.w);
            hitPixel = permute ? pqk.yx : pqk.xy;
            hitPixel *= texelSize;

            originalStride *= 0.5;
            stride = rayIntersectDepth(data, rayZNear, rayZFar, hitPixel) ? -originalStride : originalStride;
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
    alpha *= 1.0 - (iterationCount / float($SSR_MAX_ITERATIONS));
    // Fade ray hits that approach the screen edge
    vec2 hitPixelNDC = hitPixel * 2.0 - 1.0;
    float maxDimension = min(1.0, max(abs(hitPixelNDC.x), abs(hitPixelNDC.y)));
    alpha *= 1.0 - max(0.0, maxDimension - $SSR_SCREEN_EDGE_FADE_START) / (1.0 - $SSR_SCREEN_EDGE_FADE_START);

    // Fade ray hits base on how much they face the camera
    //float eyeDir = clamp(rayDir.z, $SSR_EYE_FADE_START, $SSR_EYE_FADE_END);
    //alpha *= 1.0 - (eyeDir - $SSR_EYE_FADE_START) / ($SSR_EYE_FADE_END - $SSR_EYE_FADE_START);

    // Fade ray hits based on distance from ray origin
    alpha *= 1.0 - clamp(dist / $SSR_MAX_RAY_DISTANCE, 0.0, 1.0);

    return alpha;
}

vec4 PostProcess_SSR(PostProcessData data)
{
	vec3 N = Texture2DValue(NormalMap, data.texCoord, data.resolution).rgb * 2.0 - 1.0;
    vec3 viewSpaceN = normalize((data.viewMatrix * vec4(N, 0.0)).xyz);

    // Position in view
    vec4 projectedPos = vec4(data.texCoord * 2.0 - 1.0, fetchDepth(data.texCoord, data.resolution), 1.0);
    vec4 pos = inverse(data.projMatrix) * projectedPos;
    vec3 rayOrigin = pos.xyz / pos.w;

    vec3 rayDir = normalize(reflect(normalize(rayOrigin), viewSpaceN));


	vec2 hitPixel;
    vec3 hitPoint;
    float iterationCount;

    vec2 uv2 = data.texCoord * data.resolution;
    float jitter = fract((data.texCoord.x + data.texCoord.y) * 0.25 + $SSR_JITTER_OFFSET);

    bool intersect = traceScreenSpaceRay(data, rayOrigin, rayDir, jitter, hitPixel, hitPoint, iterationCount);

    float dist = distance(rayOrigin, hitPoint);

    float alpha = calculateAlpha(iterationCount, 1.0 /* reflectivity */, hitPixel, hitPoint, dist, rayDir) * float(intersect);

    vec3 hitNormal = Texture2DValue(NormalMap, hitPixel, data.resolution).rgb * 2.0 - 1.0;
    hitNormal = normalize((data.viewMatrix * vec4(hitNormal, 0.0)).xyz);


    if (dot(hitNormal, rayDir) >= 0.0) {
        return vec4(0.0);
    }
    if (!intersect) {
        return vec4(0.0);
    }

	return Texture2DValue(ColorMap, hitPixel, data.resolution) * alpha;
}



float random (vec2 uv) {
	return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123); //simple random function
}


/*uniform float rayStep = 0.1f;
uniform int iterationCount = 100;
uniform float distanceBias = 0.02f;
uniform float offset = 0.02f;
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
}*/
/*
vec4 PostProcess_SSR2(PostProcessData data)
{
	vec2 UV = data.texCoord / data.resolution;
    float depth = Texture2DValue(DepthMap, data.texCoord, data.resolution).r;
    vec4 position = vec4(generatePositionFromDepth(data.texCoord, depth), 1.0);
	vec4 normal = u_viewMatrix * vec4(Texture2DValue(NormalMap, data.texCoord, data.resolution).xyz * 2.0 - 1.0, 0.0);
    vec4 data = Texture2DValue(DataMap, data.texCoord, data.resolution);
    float roughness = clamp(data.g, 0.05, 0.99);

	if (roughness < 0.01) {
		return vec4(0.0);
	}
	vec3 reflectionDirection = normalize(reflect(position.xyz, normalize(normal.xyz)));
	
		output0 = vec4(SSR(position.xyz, normalize(reflectionDirection + (normal.xyz * offset) )), 1.f);
	//}
}*/