#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in vec3 v_position[];
layout(location=1) in vec3 v_normal[];
layout(location=2) in vec2 v_texcoord0[];

layout(location=0) out vec3 position;
layout(location=1) out vec3 normal;
layout(location=2) out vec2 texcoord;
layout(location=3) out vec3 voxel_pos;

vec2 Project(in vec3 v, in uint axis) { return axis == 0 ? v.yz : (axis == 1 ? v.xz : v.xy); }

void main() {
	// project the positions
	vec3 pos0 = gl_in[0].gl_Position.xyz;
	vec3 pos1 = gl_in[1].gl_Position.xyz;
	vec3 pos2 = gl_in[2].gl_Position.xyz;

	// get projection axis
	vec3 axis_weight = abs(cross(pos1 - pos0, pos2 - pos0));
	uint axis = (axis_weight.x > axis_weight.y && axis_weight.x > axis_weight.z)
	                ? 0
	                : ((axis_weight.y > axis_weight.z) ? 1 : 2);

    position = pos0;
    normal = v_normal[0];
	texcoord = v_texcoord0[0];
	voxel_pos = (pos0 + 1.0f) * 0.5f;
	gl_Position = vec4(Project(pos0, axis), 1.0f, 1.0f);
	EmitVertex();
    position = pos1;
    normal = v_normal[1];
	texcoord = v_texcoord0[1];
	voxel_pos = (pos1 + 1.0f) * 0.5f;
	gl_Position = vec4(Project(pos1, axis), 1.0f, 1.0f);
	EmitVertex();
    position = pos2;
    normal = v_normal[2];
	texcoord = v_texcoord0[2];
	voxel_pos = (pos2 + 1.0f) * 0.5f;
	gl_Position = vec4(Project(pos2, axis), 1.0f, 1.0f);
	EmitVertex();
	EndPrimitive();
}
