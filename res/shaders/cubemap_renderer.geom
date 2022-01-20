#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 u_shadowMatrices[6];

// input
in VSOutput
{
    vec3 position;
	vec3 normal;
	vec2 texcoord0;
#if PROBE_RENDER_SHADING
    vec4 lighting;
#endif
} vs_in[];

// output
out GSOutput
{
	vec3 normal;
	vec2 texcoord0;
#if PROBE_RENDER_SHADING
    vec4 lighting;
#endif
} gs_out;


out vec4 FragPos; // FragPos from GS (output per emitvertex)

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face; // built-in variable that specifies to which face we render.
        for(int i = 0; i < 3; ++i) // for each triangle vertex
        {
            FragPos = vec4(vs_in[i].position, 1.0);//gl_in[i].gl_Position;
            gs_out.normal = vs_in[i].normal;
            gs_out.texcoord0 = vs_in[i].texcoord0;
#if PROBE_RENDER_SHADING
            gs_out.lighting = vs_in[i].lighting;
#endif
            gl_Position = u_shadowMatrices[face] * FragPos;
            EmitVertex();
        }    
        EndPrimitive();
    }
}  
