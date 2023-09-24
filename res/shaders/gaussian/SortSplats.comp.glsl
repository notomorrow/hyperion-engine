#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout  : require
#extension GL_EXT_nonuniform_qualifier : require

#define WORKGROUP_SIZE_X 512

layout(local_size_x = WORKGROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

#include "../include/shared.inc"
#include "../include/defines.inc"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../include/scene.inc"
#include "../include/object.inc"
#include "../include/packing.inc"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "./Gaussian.inc.glsl"

#define VALUE_TYPE GaussianSplatIndex

layout(std430, set = 0, binding = 3) buffer SplatIndicesBuffer
{
    GaussianSplatIndex splat_indices[];
};

layout(std140, set = 0, binding = 5, row_major) readonly buffer SceneShaderData
{
    Scene scene;
};

layout(std140, set = 0, binding = 6, row_major) uniform CameraShaderData
{
    Camera camera;
};

#define STAGE_LOCAL_BMS         0
#define STAGE_LOCAL_DISPERSE    1
#define STAGE_BIG_FLIP          2
#define STAGE_BIG_DISPERSE      3

layout(push_constant) uniform PushConstant {
    uint num_points;
    uint stage;
    uint h;
} push_constants;
shared VALUE_TYPE shared_data[gl_WorkGroupSize.x * 2];

#define READ_INDEX(idx) \
    splat_indices[(idx)]

#define WRITE_INDEX(idx, value) \
    splat_indices[(idx)] = (value)

#define COMPARE(a, b) ((a).distance > (b).distance)

void GlobalCompareAndSwap(ivec2 idx)
{
    VALUE_TYPE left = READ_INDEX(idx.x);
    VALUE_TYPE right = READ_INDEX(idx.y);

	if (COMPARE(left, right)) {
		WRITE_INDEX(idx.x, right);
        WRITE_INDEX(idx.y, left);
	}
}

void LocalCompareAndSwap(ivec2 idx)
{
	if (COMPARE(shared_data[idx.x], shared_data[idx.y])) {
		VALUE_TYPE tmp = shared_data[idx.x];
		shared_data[idx.x] = shared_data[idx.y];
		shared_data[idx.y] = tmp;
	}
}

void BigFlip(in uint h)
{
	uint half_h = h >> 1;

	uint q = ((gl_GlobalInvocationID.x << 1) / h) * h;
	uint x = q + (gl_GlobalInvocationID.x % half_h);
	uint y = q + h - (gl_GlobalInvocationID.x % half_h) - 1; 

	GlobalCompareAndSwap(ivec2(x, y));
}

void BigDisperse(in uint h)
{
	uint half_h = h >> 1;

	uint q = ((gl_GlobalInvocationID.x << 1) / h) * h;
	uint x = q + (gl_GlobalInvocationID.x % (half_h));
	uint y = q + (gl_GlobalInvocationID.x % (half_h)) + half_h;

	GlobalCompareAndSwap(ivec2(x, y));
}

void LocalFlip(in uint h)
{
    barrier();
    
    uint half_h = h >> 1; 
    ivec2 indices = 
        ivec2(h * ((gl_LocalInvocationID.x << 1) / h)) +
        ivec2(gl_LocalInvocationID.x % half_h, h - 1 - (gl_LocalInvocationID.x % half_h));
    
    LocalCompareAndSwap(indices);
}

void LocalDisperse(in uint h)
{
    for (; h > 1; h >>= 1) {
        barrier();

        uint half_h = h >> 1;
        ivec2 indices = ivec2(h * ((gl_LocalInvocationID.x << 1) / h))
            + ivec2(gl_LocalInvocationID.x % half_h, half_h + (gl_LocalInvocationID.x % half_h));

        LocalCompareAndSwap(indices);
    }
}

void LocalBMS(uint h)
{
    for (uint hh = 2; hh <= h; hh <<= 1) {
        LocalFlip(hh);
        LocalDisperse(hh >> 1);
    }
}

void main()
{
    if (gl_GlobalInvocationID.x >= push_constants.num_points) {
        return;
    }
 
    uint offset = (gl_WorkGroupSize.x << 1) * gl_WorkGroupID.x;

    if (push_constants.stage <= STAGE_LOCAL_DISPERSE) {
        shared_data[gl_LocalInvocationID.x << 1] = READ_INDEX(offset + (gl_LocalInvocationID.x << 1));
        shared_data[(gl_LocalInvocationID.x << 1) + 1] = READ_INDEX(offset + (gl_LocalInvocationID.x << 1) + 1);
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

        WRITE_INDEX(offset + (gl_LocalInvocationID.x << 1), shared_data[gl_LocalInvocationID.x << 1]);
        WRITE_INDEX(offset + (gl_LocalInvocationID.x << 1) + 1, shared_data[(gl_LocalInvocationID.x << 1) + 1]);
    }
}