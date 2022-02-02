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


float cubeScale = 0.3;

layout(triangle_strip, max_vertices = 24) out;
void main()
{

	vec4 centerPos = vs_out[0].position;
	mat4 mvp = u_projMatrix * u_viewMatrix;

	// -X
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	
	// +X
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));			
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// -Y
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// +Y
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));			
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// -Z
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, -.5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, -.5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();
	// +Z
	{
		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();

		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, -.5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, -.5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
		
		gl_Position = mvp * (centerPos + cubeScale * vec4(-.5, .5, .5, 0.0));
		gs_out.texcoord0 = vs_out[0].texcoord0;
		gs_out.offset = vec3(-.5, .5, .5);
		gs_out.normal = vs_out[0].normal;
		gs_out.position = vs_out[0].position;
		EmitVertex();
	}
	EndPrimitive();

}