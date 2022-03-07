#version 450

layout(location=0) out vec3 fragColor;
layout(location=1) out vec2 v_texcoord0;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;

layout(binding = 0, row_major) uniform TestDescriptor {
    mat4 model;
	mat4 pv;
} DescriptorData;

//push constants block
layout( push_constant ) uniform constants
{
	vec4 data;
} PushConstants;

void main() {
    mat4 mvp = DescriptorData.pv*DescriptorData.model;

    v_texcoord0 = a_texcoord0;
    fragColor = vec3(1.0, 0.0, 0.2);

    gl_Position = mvp * vec4(a_position, 1.0);
} 