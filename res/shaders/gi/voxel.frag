#version 430

in vec4 v_position;
in vec2 v_texcoord0;
in vec4 ndcPos;


#include "../include/matrices.inc"
#include "../include/frag_output.inc"

layout(binding = 0, rgba32f) uniform image3D framebufferImage;
uniform vec4 C_albedo;
uniform vec3 u_probePos;
uniform vec3 CameraPosition;

uniform sampler2D DiffuseMap;
uniform int HasDiffuseMap;

uniform vec3 VoxelProbePosition;

uniform mat4 StorageTransformMatrix;

uniform vec3 VoxelSceneScale;

uniform float u_intensity;

#if VCT_GEOMETRY_SHADER
// output
in GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
	vec4 position;
} gs_out;
#endif

vec3 worldToTex(vec3 world) 
{
	vec4 ss = u_projMatrix * vec4(world, 1.0);
	ss /= ss.w;
	ss.z = 0.0;
	
	return ss.xyz;

}

vec3 scaleAndBias(vec3 p) { return 0.5f * p + vec3(0.5f); }

void main(void) 
{
	float voxelImageSize = float($VCT_MAP_SIZE);
	float halfVoxelImageSize = voxelImageSize * 0.5;

	
	vec4 imageColor = C_albedo;
	
	if (HasDiffuseMap == 1) {
#if VCT_GEOMETRY_SHADER
	  imageColor = texture(DiffuseMap, gs_out.texcoord0.xy);
#endif

#if !VCT_GEOMETRY_SHADER
	  imageColor = texture(DiffuseMap, v_texcoord0);
#endif
	}
	
	//imageColor *= u_intensity;
	//imageColor *= lighting;
	//imageStore(framebufferImage, ivec3(gl_FragCoord.x/8, gl_FragCoord.y/8, gl_FragCoord.z/8), C_albedo);
	
	//vec3 voxel = scaleAndBias(gs_out.position.xyz);
	//ivec3 dim = imageSize(framebufferImage);

#if VCT_GEOMETRY_SHADER
	vec3 test_store_pos = ((gs_out.position).xyz - VoxelProbePosition) * 0.1;
	imageStore(framebufferImage, ivec3(test_store_pos.x, test_store_pos.y, test_store_pos.z)+ivec3(halfVoxelImageSize), imageColor);
#endif

#if !VCT_GEOMETRY_SHADER
	vec4 storagePos = StorageTransformMatrix * ndcPos;
	storagePos.xyz *= 1.0 / storagePos.w;
	
	ivec3 voxelPos = ivec3(imageSize(framebufferImage) * (storagePos.xyz * .5 + .5));
	//imageStore(framebufferImage, voxelPos, imageColor);
	
	
	vec3 test_store_pos = ((v_position).xyz - VoxelProbePosition) * 0.1;
	imageStore(framebufferImage, ivec3(test_store_pos.x, test_store_pos.y, test_store_pos.z)+ivec3(halfVoxelImageSize), imageColor);
#endif

	//imageStore(framebufferImage, ivec3(dim * voxel), imageColor);
}
