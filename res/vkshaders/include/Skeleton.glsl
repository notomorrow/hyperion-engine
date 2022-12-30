#ifndef HYP_SKELETON_GLSL
#define HYP_SKELETON_GLSL

#define HYP_MAX_BONES 256

struct Skeleton
{
    mat4 bones[HYP_MAX_BONES];
};

#ifndef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
layout(std140, set = HYP_DESCRIPTOR_SET_OBJECT, binding = 2, row_major) readonly buffer SkeletonBuffer
{
    Skeleton skeleton;
};
#endif

mat4 CreateSkinningMatrix(ivec4 bone_indices, vec4 bone_weights)
{
	mat4 skinning = mat4(0.0);

	int index0 = min(bone_indices.x, HYP_MAX_BONES - 1);
	skinning += bone_weights.x * skeleton.bones[index0];
	int index1 = min(bone_indices.y, HYP_MAX_BONES - 1);
	skinning += bone_weights.y * skeleton.bones[index1];
	int index2 = min(bone_indices.z, HYP_MAX_BONES - 1);
	skinning += bone_weights.z * skeleton.bones[index2];
	int index3 = min(bone_indices.w, HYP_MAX_BONES - 1);
	skinning += bone_weights.w * skeleton.bones[index3];

	return skinning;
}

#endif