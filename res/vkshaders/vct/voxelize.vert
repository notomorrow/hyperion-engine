#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=3) out vec3 v_voxel;
layout(location=4) out float v_lighting;
layout(location=5) out uint v_object_index;

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord0;
layout (location = 3) in vec2 a_texcoord1;
layout (location = 4) in vec3 a_tangent;
layout (location = 5) in vec3 a_bitangent;
layout (location = 6) in vec4 a_bone_weights;
layout (location = 7) in vec4 a_bone_indices;

#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/vct/shared.inc"

void main()
{
    vec4 position = object.model_matrix * vec4(a_position, 1.0);
    v_position = position.xyz;
    v_normal = (transpose(inverse(object.model_matrix)) * vec4(a_normal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

    /* basic n•l */
    vec3 L = light.position_intensity.xyz;
    L -= v_position.xyz * float(min(light.type, 1));
    L = normalize(L);
    vec3 N = normalize(v_normal);
    float NdotL = max(0.0001, dot(N, L));
    v_lighting = NdotL;

    v_voxel = VctWorldToAABB(position.xyz);

    v_object_index = OBJECT_INDEX;
    
    gl_Position = vec4(vec3(v_voxel.x, v_voxel.y, v_voxel.z * 0.5 + 0.5), 1.0);
}
