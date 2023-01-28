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
layout(location=5) out flat uint v_object_index;

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec2 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#include "../include/scene.inc"

#define HYP_INSTANCING
#include "../include/object.inc"

#ifdef MODE_TEXTURE_3D
    #define HYP_VCT_MODE 1
#elif defined(MODE_SVO)
    #define HYP_VCT_MODE 2
#endif

#include "../include/vct/shared.inc"

#if defined(HYP_ATTRIBUTE_a_bone_weights) && defined(HYP_ATTRIBUTE_a_bone_indices)
    #define VERTEX_SKINNING_ENABLED

    #include "../include/Skeleton.glsl"
#endif

void main()
{
    vec4 position;
    mat4 normal_matrix;

#ifdef VERTEX_SKINNING_ENABLED
        if (bool(object.flags & ENTITY_GPU_FLAG_HAS_SKELETON)) {
            mat4 skinning_matrix = CreateSkinningMatrix(ivec4(a_bone_indices), a_bone_weights);

            position = object.model_matrix * skinning_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix * skinning_matrix));
        } else
#endif
        {
            position = object.model_matrix * vec4(a_position, 1.0);
            normal_matrix = transpose(inverse(object.model_matrix));
        }

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
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
    
    gl_Position = vec4(v_voxel.xy, v_voxel.z * 0.5 + 0.5, 1.0);
}
