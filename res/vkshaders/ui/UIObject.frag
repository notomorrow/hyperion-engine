#version 450

layout(location=0) in vec3 v_position;
layout(location=1) in vec2 v_texcoord0;


layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_material;
layout(location=3) out vec4 gbuffer_tangents;

void main()
{
    gbuffer_albedo = vec4(1.0, 0.0, 0.0, 1.0);

    gbuffer_normals = vec4(0.0);
    gbuffer_material = vec4(0.0);
    gbuffer_tangents = vec4(0.0);
}
