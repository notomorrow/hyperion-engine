#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require

layout(location=0) out vec3 v_position;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_texcoord0;
layout(location=4) out vec3 v_tangent;
layout(location=5) out vec3 v_bitangent;
layout(location=11) out vec4 v_position_ndc;
layout(location=12) out vec4 v_previous_position_ndc;
layout(location=15) out flat uint v_object_index;

#ifdef IMMEDIATE_MODE
layout(location=16) out vec4 v_color;
layout(location=17) out flat uint v_probe_id;
layout(location=18) out flat uint v_probe_type;
#endif

HYP_ATTRIBUTE(0) vec3 a_position;
HYP_ATTRIBUTE(1) vec3 a_normal;
HYP_ATTRIBUTE(2) vec2 a_texcoord0;
HYP_ATTRIBUTE(3) vec3 a_texcoord1;
HYP_ATTRIBUTE(4) vec3 a_tangent;
HYP_ATTRIBUTE(5) vec3 a_bitangent;
HYP_ATTRIBUTE_OPTIONAL(6) vec4 a_bone_weights;
HYP_ATTRIBUTE_OPTIONAL(7) vec4 a_bone_indices;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "include/scene.inc"
#include "include/env_probe.inc"

#ifdef IMMEDIATE_MODE

HYP_DESCRIPTOR_SSBO(Scene, SHGridBuffer) readonly buffer SHGridBuffer { vec4 sh_grid_buffer[SH_GRID_BUFFER_SIZE]; };
HYP_DESCRIPTOR_SSBO(Scene, EnvProbesBuffer) readonly buffer EnvProbesBuffer { EnvProbe env_probes[HYP_MAX_ENV_PROBES]; };

HYP_DESCRIPTOR_SRV(Scene, LightFieldColorTexture) uniform texture2D light_field_color_texture;
HYP_DESCRIPTOR_SRV(Scene, LightFieldDepthTexture) uniform texture2D light_field_depth_texture;

HYP_DESCRIPTOR_SSBO_DYNAMIC(DebugDrawerDescriptorSet, ImmediateDrawsBuffer, size = 80) readonly buffer ImmediateDrawsBuffer
{
    mat4 model_matrix;

    uint color_packed;
    uint probe_type;
    uint probe_id;
    uint _pad2;
};

#define MODEL_MATRIX (model_matrix)
#define PREV_MODEL_MATRIX (model_matrix)
#else
#include "include/object.inc"
HYP_DESCRIPTOR_SSBO(Scene, ObjectsBuffer) readonly buffer ObjectsBuffer
{
    Object objects[HYP_MAX_ENTITIES];
};

HYP_DESCRIPTOR_SSBO_DYNAMIC(Instancing, EntityInstanceBatchesBuffer) readonly buffer EntityInstanceBatchesBuffer
{
    EntityInstanceBatch entity_instance_batch;
};

#define MODEL_MATRIX (object.model_matrix)
#define PREV_MODEL_MATRIX (object.previous_model_matrix)
#endif
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

HYP_DESCRIPTOR_CBUFF_DYNAMIC(Scene, CamerasBuffer) uniform CamerasBuffer
{
    Camera camera;
};

void main()
{
    vec4 position = MODEL_MATRIX * vec4(a_position, 1.0);
    vec4 previous_position = PREV_MODEL_MATRIX * vec4(a_position, 1.0);

    mat4 normal_matrix = transpose(inverse(MODEL_MATRIX));

    v_position = position.xyz;
    v_normal = (normal_matrix * vec4(a_normal, 0.0)).xyz;
    v_tangent = (normal_matrix * vec4(a_tangent, 0.0)).xyz;
	v_bitangent = (normal_matrix * vec4(a_bitangent, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;

#ifdef IMMEDIATE_MODE
    v_object_index = ~0u; // unused
    v_color = UINT_TO_VEC4(color_packed);

    v_probe_id = probe_id;
    v_probe_type = probe_type;

    if (probe_id != 0 && probe_type == ENV_PROBE_TYPE_AMBIENT)
    {
        const int storage_index = env_probes[probe_id - 1].position_in_grid.w * 9;

        SH9 sh9;

        for (int j = 0; j < 9; j++) {
            sh9.values[j] = sh_grid_buffer[min(storage_index + j, SH_GRID_BUFFER_SIZE - 1)].rgb;
        }

        v_color = vec4(SphericalHarmonicsSample(sh9, a_normal), 1.0);
    }
#else
    v_object_index = OBJECT_INDEX;
#endif

    mat4 jitter_matrix = mat4(1.0);
    jitter_matrix[3][0] += camera.jitter.x;
    jitter_matrix[3][1] += camera.jitter.y;

    v_position_ndc = (jitter_matrix * camera.projection) * camera.view * position;
    v_previous_position_ndc = (jitter_matrix * camera.projection) * camera.previous_view * previous_position;

    gl_Position = camera.projection * camera.view * position;
} 