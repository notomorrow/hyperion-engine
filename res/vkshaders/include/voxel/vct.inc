layout(std430, set = 8, binding = 2) readonly buffer uuOctree { uvec2 uOctree[]; };

#include "shared.inc"

const float uVolumeDimension = 1024.0;
#define VCT_ORIGIN_OFFSET 0.4

const int 	DIFFUSE_CONE_COUNT_16 		= 16;
const float DIFFUSE_CONE_APERTURE_16 	= 0.872665;

const vec3 DIFFUSE_CONE_DIRECTIONS_16[16] = {
    vec3( 0.57735,   0.57735,   0.57735  ),
    vec3( 0.57735,  -0.57735,  -0.57735  ),
    vec3(-0.57735,   0.57735,  -0.57735  ),
    vec3(-0.57735,  -0.57735,   0.57735  ),
    vec3(-0.903007, -0.182696, -0.388844 ),
    vec3(-0.903007,  0.182696,  0.388844 ),
    vec3( 0.903007, -0.182696,  0.388844 ),
    vec3( 0.903007,  0.182696, -0.388844 ),
    vec3(-0.388844, -0.903007, -0.182696 ),
    vec3( 0.388844, -0.903007,  0.182696 ),
    vec3( 0.388844,  0.903007, -0.182696 ),
    vec3(-0.388844,  0.903007,  0.182696 ),
    vec3(-0.182696, -0.388844, -0.903007 ),
    vec3( 0.182696,  0.388844, -0.903007 ),
    vec3(-0.182696,  0.388844,  0.903007 ),
    vec3( 0.182696, -0.388844,  0.903007 )
};

vec4 sampleSVO(vec3 position, vec3 aabb_max, vec3 aabb_min, float lod)
{
	vec3 extent = aabb_max - aabb_min;
    float extentValue = max(extent.x, max(extent.y, extent.z)) * 0.5;
    vec3 center = (aabb_min + aabb_max) * 0.5;

	uint resolution = uint(uVolumeDimension);
	vec3  scaledPos = (position - center) / extentValue;

    uvec3 fragPos = clamp(uvec3(((scaledPos + 1.0) * 0.5) * resolution), 
						  uvec3(0u), 
						  uvec3(resolution));


    uint targetResolution = 0;//uint(mix(0, 1 << 3, clamp(lod, 0.0, 7.0) / 7.0));
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

vec4 voxelTraceCone(vec3 origin, vec3 dir, vec3 aabb_max, vec3 aabb_min, float coneRatio, float maxDist)
{
	float minVoxelDiameter = 1.0 / uVolumeDimension;
	float minVoxelDiameterInv = 1.0 / minVoxelDiameter;
	vec3 samplePos;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(0.0, 0.0, 0.0, 0.0);

	origin += dir * VCT_ORIGIN_OFFSET;

	while (dist <= maxDist && accum.w < 1.0) {
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		samplePos = origin + dir * dist;
		vec4 sampleValue = sampleSVO(samplePos, aabb_max, aabb_min, sampleLOD);
		sampleValue = mix(sampleValue, vec4(sampleValue.rgb, 0.0), clamp(dist / maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}

	return accum;
}