layout(std430, set = 8, binding = 2) readonly buffer uuOctree { uvec2 uOctree[]; };

#include "shared.inc"

const float uVoxelSize = 1.0;
const float uTraceStartOffset = 0.7;
const float uVolumeDimension = 64.0;
const float uOcclusionDecay = 0.1;
#define CLIP_LEVEL_COUNT 6
#define VCT_STEP_FACTOR 0.5
#define VCT_MAX_DISTANCE 3.0

vec4 sampleSVO(vec3 position, vec3 aabb_max, vec3 aabb_min, uint level, vec3 weight)
{
	vec3 extent = aabb_max - aabb_min;
    float extentValue = max(extent.x, max(extent.y, extent.z)) * 0.5;
    vec3 center = (aabb_min + aabb_max) * 0.5;

	uint resolution = uint(uVolumeDimension);
	vec3  scaledPos = (position - center) / extentValue;

    uvec3 fragPos = clamp(uvec3(((scaledPos + 1.0) * 0.5) * resolution), 
						  uvec3(0u), 
						  uvec3(resolution));

    uint targetResolution = 1 << level;
	uint idx = 0, cur = 0; 
	uvec3 cmp;
	do {
		resolution >>= 1;
		cmp = uvec3(greaterThanEqual(fragPos, uvec3(resolution)));
		idx = cur + (uint(cmp.x) | (uint(cmp.y) << 1u) | (uint(cmp.z) << 2u));
		cur = uOctree[idx].x & 0x7fffffffu;
		fragPos -= cmp * resolution;
	} while (cur != 0u && resolution > targetResolution);

	return RGBA8ToVec4Raw(uOctree[idx].y) / 255.0;
}

vec4 sampleSVOLinear(vec3 position, vec3 aabb_max, vec3 aabb_min, float level, vec3 weight)
{
	uint lowerLevel = uint(floor(level));
	uint upperLevel = uint( ceil(level));

	vec4 lowerSample = sampleSVO(position, aabb_max, aabb_min, lowerLevel, weight);
	vec4 upperSample = sampleSVO(position, aabb_max, aabb_min, upperLevel, weight);

	return mix(lowerSample, upperSample, fract(level));
}

float calcMinLevel(vec3 worldPos, vec3 aabb_max, vec3 aabb_min)
{
    vec3 uVolumeCenter = vec3(0.0);//((aabb_min + aabb_max) / 2.0) * uVoxelSize;
    
	float distanceToVoxelCenter = length(uVolumeCenter - worldPos);
	float minRadius = uVoxelSize * uVolumeDimension * 0.5;
	float minLevel = max(log2(distanceToVoxelCenter / minRadius), 0.0);

	float radius = minRadius * exp2(ceil(minLevel));
	float f = distanceToVoxelCenter / radius;

	const float transitionStart = 0.5;
	float c = 1.0 / (1.0 - transitionStart);

	if (f > transitionStart)
	{
		return ceil(minLevel) + (f - transitionStart) * c;
	}
	else 
	{
		return ceil(minLevel);
	}
}

vec4 traceCone(vec3 startPos, vec3 direction, vec3 aabb_max, vec3 aabb_min, float aperture, float maxDistance, float startLevel, float stepFactor)
{
	vec4 result = vec4(0.0);
	float coneCoefficient = 2.0 * tan(aperture * 0.5);

	float curLevel = startLevel;
	float voxelSize = uVoxelSize * exp2(curLevel);

	startPos += direction * voxelSize * uTraceStartOffset * 0.5;

	float step 		 = 0.0;
	float diameter 	 = max(step * coneCoefficient, uVoxelSize);
	float occlusion  = 0.0;

	vec3  weight 	 = direction * direction;

	float curSegmentLength 	= voxelSize;
	float minRadius 		= uVoxelSize * uVolumeDimension * 0.5;
    
    vec3 uVolumeCenter = vec3(0.0);//((aabb_min + aabb_max) / 2.0) * uVoxelSize;

	for( ; step < maxDistance && occlusion < 1.0; )
	{
		vec3  position 				= startPos + direction * step;
		float distanceToVoxelCenter = length(uVolumeCenter - position);
		float minLevel 				= ceil(log2(distanceToVoxelCenter / minRadius));

		curLevel = log2(diameter / uVoxelSize);
		curLevel = min(max(max(startLevel, curLevel), minLevel), CLIP_LEVEL_COUNT - 1);

		vec4 clipmapSample = sampleSVOLinear(position, aabb_max, aabb_min, curLevel, weight);
		vec3 radiance = clipmapSample.rgb;
		float opacity = clipmapSample.a;

		voxelSize = uVoxelSize * exp2(curLevel);

		float correction = curSegmentLength / voxelSize;
		radiance = radiance * correction;
		opacity  = clamp(1.0 - pow(1.0 - opacity, correction), 0.0, 1.0);

		vec4 src = vec4(radiance.rgb, opacity);
		// Alpha blending
		result 	  += clamp(1.0 - result.a, 0.0, 1.0) * src;
		occlusion += (1.0 - occlusion) * opacity / (1.0 + (step + voxelSize) * uOcclusionDecay);

		float prevStep = step;
		step += max(diameter, uVoxelSize) * stepFactor;
		curSegmentLength = (step - prevStep);
		diameter = step * coneCoefficient;
	}

	return clamp(vec4(result.rgb, 1.0 - occlusion), 0.0, 1.0);
}