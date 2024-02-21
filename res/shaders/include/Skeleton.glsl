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

#endif