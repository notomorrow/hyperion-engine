#version 430

uniform mat4 u_modelMatrix;
uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;


layout(triangles) in;
//layout(triangle_strip, max_vertices = 24) out;
layout(triangle_strip, max_vertices = 3) out;
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


float cubeScale = 0.35;

#define $OLD 1

#if OLD
void main()
{

	vec4 centerPos = gl_in[0].gl_Position;
	mat4 mvp = u_projMatrix * u_viewMatrix * u_modelMatrix;

	// -X
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	
	// +X
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));			
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// -Y
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// +Y
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));			
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// -Z
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// +Z
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = u_modelMatrix * vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();

}
#endif

#if NEW
void main() {
    const vec3 p1 = (vs_out[1].position - vs_out[0].position).xyz;
	const vec3 p2 = (vs_out[2].position - vs_out[0].position).xyz;
	const vec3 p = abs(cross(p1, p2)); 
	for(uint i = 0; i < 3; ++i){
		gs_out.position = vs_out[i].position;
		gs_out.normal = vs_out[i].normal;
		gs_out.texcoord0 = vs_out[i].texcoord0;
		if(p.z > p.x && p.z > p.y){
			gl_Position = vec4(gs_out.position.x, gs_out.position.y, 0.0, 1.0);
		} else if (p.x > p.y && p.x > p.z){
			gl_Position = vec4(gs_out.position.y, gs_out.position.z, 0.0, 1.0);
		} else {
			gl_Position = vec4(gs_out.position.x, gs_out.position.z, 0.0, 1.0);
		}
		EmitVertex();
	}
    EndPrimitive();

}
#endif