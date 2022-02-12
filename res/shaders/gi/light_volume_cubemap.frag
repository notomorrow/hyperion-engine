#version 430 core
in vec4 FragPos;

#include "../include/frag_output.inc"

layout(binding = 0, rgba8) uniform image3D lightVolumeGrid;
uniform vec3 LightVolumeGridOffset;

uniform int HasDiffuseMap;
uniform vec4 u_diffuseColor;
uniform sampler2D DiffuseMap;

in GSOutput
{
	vec3 normal;
	vec2 texcoord0;
    vec4 lighting;
} gs_out;

//in int cubeFace;

void main()
{
    vec4 albedo = u_diffuseColor;

#if PROBE_RENDER_TEXTURES
    if (HasDiffuseMap == 1) {
        albedo *= texture(DiffuseMap, gs_out.texcoord0);
    }
#endif

#if PROBE_RENDER_SHADING
    output0 = albedo * gs_out.lighting;
#endif

#if !PROBE_RENDER_SHADING
    output0 = albedo;
#endif

	/*ivec3 faceOffset = ivec3(0, 0, 0);
	
	if (cubeFace == 1) { // neg x
		faceOffset = ivec3(1, 0, 0);
	} else if (cubeFace == 2) { // pos y
		faceOffset = ivec3(0, 1, 0);
	} else if (cubeFace == 3) {
		faceOffset = ivec3(0, 0, 1); // neg y
	} else if (cubeFace == 4) {
		faceOffset = ivec3(1, 1, 0); // pos z
	} else if (cubeFace == 5) { // neg z
		faceOffset = ivec3(0, 1, 1);
	}

	imageStore(lightVolumeGrid, ivec3(LightVolumeGridOffset) + faceOffset, output0);*/
	imageStore(lightVolumeGrid, ivec3(LightVolumeGridOffset), vec4(1.0, 0.0, 0.0, 1.0));

}  
