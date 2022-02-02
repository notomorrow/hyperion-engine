#if !VCT_ENABLED
#error "VCT_ENABLED must be set to true to include this header"
#endif

// TODO: make this configurable, more mapped to bounding box
#define $VCT_SCALE 0.1
#define $VCT_SPECULAR_MAX_LEN 20.0
#define $VCT_SPECULAR_CONE_SIZE 0.1

uniform sampler3D VoxelMap[$VCT_NUM_MAPS];
uniform vec3 VoxelProbePosition;
uniform mat4 WorldToVoxel;

in vec4 v_voxelPosition; // from vertex shader

vec4 voxelFetch(vec3 pos, vec3 dir, float lod)
{
	vec4 sampleX =
		dir.x < 0.0
		? textureLod(VoxelMap[0], pos, lod)
		: textureLod(VoxelMap[1], pos, lod);
	
	vec4 sampleY =
		dir.y < 0.0
		? textureLod(VoxelMap[2], pos, lod)
		: textureLod(VoxelMap[3], pos, lod);
	
	vec4 sampleZ =
		dir.z < 0.0
		? textureLod(VoxelMap[4], pos, lod)
		: textureLod(VoxelMap[5], pos, lod);
	
	vec3 sampleWeights = abs(dir);
	float invSampleMag = 1.0 / (sampleWeights.x + sampleWeights.y + sampleWeights.z + .0001);
	sampleWeights *= invSampleMag;
	
	vec4 filtered = 
		sampleX * sampleWeights.x
		+ sampleY * sampleWeights.y
		+ sampleZ * sampleWeights.z;

	return filtered;
}

vec3 scaleAndBias(const vec3 p) { return 0.5f * p + vec3(0.5f); }

vec4 voxelTraceCone(float minVoxelDiameter, vec3 origin, vec3 dir, float coneRatio, float maxDist)
{
	float minVoxelDiameterInv = 1.0/minVoxelDiameter;
	vec3 samplePos = origin;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(1.0, 1.0, 1.0, 1.0)*0.2;
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		vec3 samplePos = origin + dir * dist;
		vec4 sampleValue = voxelFetch(samplePos, -dir, sampleLOD);
		sampleValue = mix(sampleValue,fadeCol, clamp(dist/maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}
	return accum;
}


vec3 decodeVoxelPosition()
{
	//return scaleAndBias(v_voxelPosition.xyz);
	vec4 voxelPos = WorldToVoxel * vec4(v_voxelPosition.xyz, 1.0);
	voxelPos.xyz *= 1.0 / voxelPos.w;
	//return voxelPos.xyz;
	
	float voxelImageSize = float($VCT_MAP_SIZE);
	float halfVoxelImageSize = voxelImageSize * 0.5;

	return (vec3((v_voxelPosition.xyz - VoxelProbePosition) * $VCT_SCALE) + vec3(halfVoxelImageSize)) / vec3(voxelImageSize);
}

vec4 VCTSpecular(vec3 position, vec3 normal, vec3 camera)
{
	float voxelImageSize = float($VCT_MAP_SIZE);

	vec3 viewVector = normalize(position - camera);
	vec3 reflectionDir = reflect(viewVector, normalize(normal));
	
	vec3 voxelPosition = decodeVoxelPosition();
	
	return voxelTraceCone(1.0 / voxelImageSize, voxelPosition, normalize(reflectionDir), $VCT_SPECULAR_CONE_SIZE, $VCT_SPECULAR_MAX_LEN);
}