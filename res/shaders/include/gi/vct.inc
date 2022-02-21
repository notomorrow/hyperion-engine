#if !VCT_ENABLED
#error "VCT_ENABLED must be set to true to include this header"
#endif

#define $VCT_SPECULAR_MAX_LEN 0.3
#define $VCT_SPECULAR_CONE_SIZE 0.07
#define $VCT_DIFFUSE_MAX_LEN 0.2
#define $VCT_DIFFUSE_CONE_SIZE 1.5
#define $VCT_NORMAL_OFFSET 0.015

uniform sampler3D VoxelMap;
uniform vec3 VoxelProbePosition;
uniform mat4 WorldToVoxel;

vec4 voxelFetch(vec3 pos, vec3 dir, float lod)
{
	return textureLod(VoxelMap, pos, lod);
}

vec4 voxelTraceCone(float minVoxelDiameter, vec3 origin, vec3 dir, float coneRatio, float maxDist)
{
	float minVoxelDiameterInv = 1.0/minVoxelDiameter;
	vec3 samplePos = origin;
	vec4 accum = vec4(0.0);
	float minDiameter = minVoxelDiameter;
	float startDist = minDiameter;
	float dist = startDist;
	vec4 fadeCol = vec4(0.0, 0.0, 0.0, 0.0);
	while (dist <= maxDist && accum.w < 1.0)
	{
		float sampleDiameter = max(minDiameter, coneRatio * dist);
		float sampleLOD = log2(sampleDiameter * minVoxelDiameterInv);
		vec3 samplePos = origin + dir * dist;
		vec4 sampleValue = voxelFetch(samplePos, -dir, sampleLOD);
		sampleValue = mix(sampleValue,vec4(sampleValue.rgb, 0.0), clamp(dist/maxDist, 0.0, 1.0));
		float sampleWeight = (1.0 - accum.w);
		accum += sampleValue * sampleWeight;
		dist += sampleDiameter;
	}
	return accum;
}


vec3 decodeVoxelPosition(vec3 voxelPosition)
{
	//return scaleAndBias(v_voxelPosition.xyz);
	vec4 voxelPos = WorldToVoxel * vec4(voxelPosition, 1.0);
	voxelPos.xyz *= 1.0 / voxelPos.w;
	//return voxelPos.xyz;
	
	float voxelImageSize = float($VCT_MAP_SIZE);
	float halfVoxelImageSize = voxelImageSize * 0.5;

	return clamp((vec3((voxelPosition - VoxelProbePosition) * $VCT_SCALE) + vec3(halfVoxelImageSize)) / vec3(voxelImageSize), vec3(0.0), vec3(1.0));
}

vec4 VCTDiffuse(vec3 position, vec3 normal, vec3 camera, vec3 tangent, vec3 bitangent, float roughness)
{
	float voxelImageSize = float($VCT_MAP_SIZE);
	
	vec3 voxelPosition = decodeVoxelPosition(position) + (normal * $VCT_NORMAL_OFFSET);
	
	float roughnessConversion = mix(0.0, 2.0, roughness);

	vec4 coneTraceRes = vec4(0.0);

	coneTraceRes += voxelTraceCone(1.0/voxelImageSize, voxelPosition, normalize(normal), $VCT_DIFFUSE_CONE_SIZE + roughnessConversion, $VCT_DIFFUSE_MAX_LEN);
	coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, voxelPosition, normalize(normal+tangent), $VCT_DIFFUSE_CONE_SIZE + roughnessConversion, $VCT_DIFFUSE_MAX_LEN);
	coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, voxelPosition, normalize(normal-tangent), $VCT_DIFFUSE_CONE_SIZE + roughnessConversion, $VCT_DIFFUSE_MAX_LEN);
	coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, voxelPosition, normalize(normal+bitangent), $VCT_DIFFUSE_CONE_SIZE + roughnessConversion, $VCT_DIFFUSE_MAX_LEN);
	coneTraceRes += 0.707 * voxelTraceCone(1.0/voxelImageSize, voxelPosition, normalize(normal-bitangent), $VCT_DIFFUSE_CONE_SIZE + roughnessConversion, $VCT_DIFFUSE_MAX_LEN);
	
	return coneTraceRes / 5.0;
}

vec4 VCTSpecular(vec3 position, vec3 normal, vec3 camera, float roughness)
{
	float voxelImageSize = float($VCT_MAP_SIZE);

	vec3 viewVector = normalize(position - camera);
	vec3 reflectionDir = reflect(viewVector, normalize(normal));
	
	vec3 voxelPosition = decodeVoxelPosition(position) + (normal * $VCT_NORMAL_OFFSET);
	
	float roughnessConversion = mix(0.0, 2.0, roughness);
	
	vec4 spec = voxelTraceCone(1.0 / voxelImageSize, voxelPosition, normalize(reflectionDir), $VCT_SPECULAR_CONE_SIZE + roughnessConversion, $VCT_SPECULAR_MAX_LEN);

	spec = CalculateFogLinear(spec, vec4(0.0), position, VoxelProbePosition, (voxelImageSize * 0.5 / $VCT_SCALE) - 10.0, voxelImageSize * 0.5 / $VCT_SCALE);

	return spec;
}