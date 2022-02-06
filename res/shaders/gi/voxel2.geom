#version 430

uniform mat4 u_modelMatrix;
uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;


layout(triangles) in;
//
// input
in VSOutput
{
	vec4 position;
	vec3 normal;
	vec3 texcoord0;
} vs_out[];

// output
out GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
	vec4 position;
} gs_out;


float cubeScale = 2.3;

layout(triangle_strip, max_vertices = 3) out;
void main()
{

	const vec3 p1 = vs_out[1].position.xyz - vs_out[0].position.xyz;
	const vec3 p2 = vs_out[2].position.xyz - vs_out[0].position.xyz;
	const vec3 p = abs(cross(p1, p2)); 
	for(uint i = 0; i < 3; ++i){
		gs_out.position = vs_out[i].position;
		gs_out.normal = vs_out[i].normal;
		gs_out.texcoord0 = vs_out[i].texcoord0;
		if(p.z > p.x && p.z > p.y){
			gl_Position = vec4(gs_out.position.x, gs_out.position.y, 0, 1);
		} else if (p.x > p.y && p.x > p.z){
			gl_Position = vec4(gs_out.position.y, gs_out.position.z, 0, 1);
		} else {
			gl_Position = vec4(gs_out.position.x, gs_out.position.z, 0, 1);
		}
		EmitVertex();
	}
    EndPrimitive();

}