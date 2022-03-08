#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 fragColor;
layout(location=1) in vec2 v_texcoord0;

layout(location=0) out vec4 outColor;

layout(binding = 1) uniform sampler2D tex;

layout(set = 1, binding = 0) uniform sampler2D fboTex;

void main() {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);//vec4(texture(fboTex, v_texcoord0).rgb, 1.0);
}