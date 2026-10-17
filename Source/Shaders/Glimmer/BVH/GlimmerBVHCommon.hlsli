#ifndef GLIMMER_BVH_COMMON_HLSLI
#define GLIMMER_BVH_COMMON_HLSLI

struct GlimmerTracedInstance
{
    float4 worldToObject0;
    float4 worldToObject1;
    float4 worldToObject2;

    /// x = BLAS node base
    /// y = BLAS triangle base
    /// z = material index
    /// w = flags
    uint4 data;
};

#endif
