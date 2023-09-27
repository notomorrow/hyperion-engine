#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

#include "../include/shared.inc"
#include "../include/defines.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/packing.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.inc.glsl"

struct IndirectDrawCommand
{
    // VkDrawIndexedIndirectCommand
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
};

layout(set = 0, binding = 1) buffer IndirectDrawCommandsBuffer
{
    IndirectDrawCommand indirect_draw_command;
};

layout(std140, set = 0, binding = 5, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

layout(std140, set = 0, binding = 6, row_major) uniform CameraShaderData
{
    Camera camera;
};

layout(push_constant) uniform PushConstant {
    uint num_points;  
} push_constants;

void main()
{
    const uint id = gl_GlobalInvocationID.x;
    const uint index = id;

    if (id >= push_constants.num_points) {
        return;
    }

    GaussianSplatShaderData instance = instances[index];

    atomicAdd(indirect_draw_command.instance_count, 1u);
}