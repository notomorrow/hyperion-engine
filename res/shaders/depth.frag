#version 330 core

in vec4 v_position;
in vec4 v_normal;
in vec2 v_texcoord0;
in vec3 v_tangent;
in vec3 v_bitangent;
in mat3 v_tbn;

uniform vec3 u_camerapos;

#include "include/depth.inc"
#include "include/frag_output.inc"

void main()
{
    float depthCoord = gl_FragCoord.z / gl_FragCoord.w;

#if SHADOWS_VARIANCE
    float moment2 = depthCoord * depthCoord;
    output0 = vec4(depthCoord, moment2, 0.0, 1.0);
#endif

#if !SHADOWS_VARIANCE
#if SHADOWS_PACK_DEPTH
    output0 = packDepth(depthCoord);
#endif

#if !SHADOWS_PACK_DEPTH
    output0 = vec4(depthCoord, depthCoord, depthCoord, 1.0);
#endif
#endif
}
