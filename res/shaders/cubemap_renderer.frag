#version 330 core
in vec4 FragPos;

uniform vec3 u_lightPos;
uniform float u_far;

#include "include/frag_output.inc"

uniform int HasDiffuseMap;
uniform vec4 u_diffuseColor;
uniform sampler2D DiffuseMap;

in GSOutput
{
	vec3 normal;
	vec2 texcoord0;
    vec4 lighting;
} gs_out;

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
}  
