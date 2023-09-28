#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

#if !defined(MODE_SORT) && !defined(MODE_TRANSPOSE)
    #define MODE_SORT
#endif

#ifdef MODE_SORT
    #define WORKGROUP_SIZE_X 1024

    layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;
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

#define STAGE_LOCAL_BMS 0
#define STAGE_LOCAL_DISPERSE 1
#define STAGE_BIG_FLIP 2
#define STAGE_BIG_DISPERSE 3

layout(push_constant) uniform PushConstant {
    uint num_points;
    uint level;
    uint level_mask;
    uint width;
    uint height;
    uint stage;
    uint h;
} push_constants;

#ifdef MODE_SORT

shared uint shared_data[gl_WorkGroupSize.x * 2];

#define READ_INDEX(idx) \
    splat_indices[(idx) >> 2][(idx) & 3]

#define WRITE_INDEX(idx, value) \
    splat_indices[(idx) >> 2][(idx) & 3] = (value)

#define COMPARE(a, b) ((a) < (b))

void GlobalCompareAndSwap(ivec2 idx)
{
    uint left = READ_INDEX(idx.x);
    uint right = READ_INDEX(idx.y);

	if (COMPARE(left, right)) {
		WRITE_INDEX(idx.x, right);
        WRITE_INDEX(idx.y, left);
	}
}

// Performs compare-and-swap over elements held in shared,
// workgroup-local memory
void LocalCompareAndSwap(ivec2 idx)
{
	if (COMPARE(shared_data[idx.x], shared_data[idx.y])) {
		uint tmp = shared_data[idx.x];
		shared_data[idx.x] = shared_data[idx.y];
		shared_data[idx.y] = tmp;
	}
}

// Performs full-height flip (h height) over globally available indices.
void BigFlip(in uint h)
{
	uint t_prime = gl_GlobalInvocationID.x;
	uint half_h = h >> 1;

	uint q = ((2 * t_prime) / h) * h;
	uint x = q + (t_prime % half_h);
	uint y = q + h - (t_prime % half_h) - 1; 

	GlobalCompareAndSwap(ivec2(x, y));
}

// Performs full-height disperse (h height) over globally available indices.
void BigDisperse(in uint h)
{
	uint t_prime = gl_GlobalInvocationID.x;

	uint half_h = h >> 1;

	uint q = ((2 * t_prime) / h) * h;
	uint x = q + (t_prime % (half_h));
	uint y = q + (t_prime % (half_h)) + half_h;

	GlobalCompareAndSwap(ivec2(x, y));
}

// Performs full-height flip (h height) over locally available indices.
void LocalFlip(in uint h)
{
    uint t = gl_LocalInvocationID.x;
    barrier();
    
    uint half_h = h >> 1; // Note: h >> 1 is equivalent to h / 2 
    ivec2 indices = 
        ivec2(h * ((2 * t) / h)) +
        ivec2(t % half_h, h - 1 - (t % half_h));
    
    LocalCompareAndSwap(indices);
}

// Performs progressively diminishing disperse operations (starting with height h)
// on locally available indices: e.g. h==8 -> 8 : 4 : 2.
// One disperse operation for every time we can divide h by 2.
void LocalDisperse(in uint h)
{
    uint t = gl_LocalInvocationID.x;
    for (; h > 1 ; h /= 2 ) {
        barrier();

        uint half_h = h >> 1;
        ivec2 indices = ivec2(h * ((2 * t) / h)) + ivec2(t % half_h, half_h + (t % half_h));

        LocalCompareAndSwap(indices);
    }
}

// Perform binary merge sort for local elements, up to a maximum number 
// of elements h.
void LocalBMS(uint h)
{
    uint t = gl_LocalInvocationID.x;
    for (uint hh = 2; hh <= h; hh <<= 1) {
        LocalFlip(hh);
        LocalDisperse(hh / 2);
    }
}

void main()
{
    uint t = gl_LocalInvocationID.x;
 
    uint offset = gl_WorkGroupSize.x * 2 * gl_WorkGroupID.x;

    if (push_constants.stage <= STAGE_LOCAL_DISPERSE) {
        shared_data[t*2] = READ_INDEX(offset + t * 2);
        shared_data[t*2+1] = READ_INDEX(offset + t * 2 + 1);
    }

    switch (push_constants.stage) {
    case STAGE_LOCAL_BMS:
        LocalBMS(push_constants.h);
        break;
    case STAGE_LOCAL_DISPERSE:
        LocalDisperse(push_constants.h);
        break;
    case STAGE_BIG_FLIP:
        BigFlip(push_constants.h);
        break;
    case STAGE_BIG_DISPERSE:
        BigDisperse(push_constants.h);
        break;
    }

    if (push_constants.stage <= STAGE_LOCAL_DISPERSE) {
        barrier();

        WRITE_INDEX(offset + t * 2, shared_data[t * 2]);
        WRITE_INDEX(offset + t * 2 + 1, shared_data[t * 2 + 1]);
    }
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