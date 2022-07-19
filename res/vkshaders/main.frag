#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;
layout(location=7) in vec3 v_camera_position;

layout(location=0) out vec4 outColor;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 0) uniform sampler2D fboTex;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}