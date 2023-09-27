#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

#if !defined(MODE_SORT) && !defined(MODE_TRANSPOSE)
    #define MODE_SORT
#endif

#ifdef MODE_SORT
    #define BLOCK_SIZE 512

    layout(local_size_x = BLOCK_SIZE, local_size_y = 1, local_size_z = 1) in;
#elif defined(MODE_TRANSPOSE)
    #define BLOCK_SIZE 16

    layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;
#endif


#include "../include/shared.inc"
#include "../include/defines.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/packing.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.inc.glsl"

layout(std430, set = 0, binding = 3) buffer SplatIndicesBuffer
{
    uvec4 splat_indices[];
};

#define READ_DISTANCE(idx) \
    splat_distances[(idx) >> 2][(idx) & 3]

layout(std430, set = 0, binding = 4) readonly buffer SplatDistancesBuffer
{
    vec4 splat_distances[];
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
    uint level;
    uint level_mask;
    uint width;
    uint height;
} push_constants;

#ifdef MODE_SORT

shared uint shared_data[BLOCK_SIZE];

#define READ_INDEX(idx) \
    splat_indices[(idx) >> 2][(idx) & 3]

#define WRITE_INDEX(idx, value) \
    splat_indices[(idx) >> 2][(idx) & 3] = (value)

void main()
{
    ivec2 dispatch_thread_id = ivec2(gl_GlobalInvocationID.xy);
    ivec2 local_invocation_id = ivec2(gl_LocalInvocationID.xy);

    if (dispatch_thread_id.x >= push_constants.num_points) {
        return;
    }

    // Load shared data
    shared_data[gl_LocalInvocationIndex] = READ_INDEX(gl_GlobalInvocationID.x);
    barrier();

    // Sort the shared data
    for (uint j = push_constants.level >> 1; j > 0; j >>= 1) {
        uint result = (((shared_data[gl_LocalInvocationIndex & ~j]) <= (shared_data[gl_LocalInvocationIndex | j])) == bool(push_constants.level_mask & gl_GlobalInvocationID.x))
            ? shared_data[gl_LocalInvocationIndex ^ j]
            : shared_data[gl_LocalInvocationIndex];
        barrier();
        shared_data[gl_LocalInvocationIndex] = result;
        barrier();
    }

    // Store shared data
    WRITE_INDEX(gl_GlobalInvocationID.x, shared_data[gl_LocalInvocationIndex]);
}

#elif defined(MODE_TRANSPOSE)

layout(std430, set = 0, binding = 9) readonly buffer SortSplatsInputBuffer
{
    uvec4 input_data[];
};

layout(std430, set = 0, binding = 10) buffer SortSplatsOutputBuffer
{
    uvec4 output_data[];
};

#define READ_INDEX(idx) \
    input_data[(idx) >> 2][(idx) & 3]

#define WRITE_INDEX(idx, value) \
    output_data[(idx) >> 2][(idx) & 3] = (value)

shared uint shared_data[BLOCK_SIZE * BLOCK_SIZE];

void main()
{
    ivec2 dispatch_thread_id = ivec2(gl_GlobalInvocationID.xy);
    ivec2 local_invocation_id = ivec2(gl_LocalInvocationID.xy);

	uvec2 xy = dispatch_thread_id.yx - local_invocation_id.yx + local_invocation_id.xy;

    uint write_index = xy.y * push_constants.height + xy.x;
    uint read_index = dispatch_thread_id.y * push_constants.width + dispatch_thread_id.x;

    if (write_index >= push_constants.num_points || read_index >= push_constants.num_points) {
        return;
    }

    uint gi = gl_LocalInvocationIndex;

    shared_data[gi] = READ_INDEX(read_index);

	barrier();

	WRITE_INDEX(write_index, shared_data[local_invocation_id.x * BLOCK_SIZE + local_invocation_id.y]);
}

#endif