struct VSInput
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal : NORMAL0;
    [[vk::location(2)]] float2 texcoord0 : TEXCOORD0;
    [[vk::location(3)]] float2 texcoord1 : TEXCOORD1;
    [[vk::location(4)]] float3 tangent : TANGENT0;
    [[vk::location(5)]] float3 binormal : BINORMAL0;
    [[vk::location(6)]] float4 bone_weights : BLENDWEIGHT0;
    [[vk::location(7)]] float4 bone_indices : BLENDINDICES0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    // [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(0)]] float3 normal : NORMAL0;
    [[vk::location(1)]] float2 texcoord0 : TEXCOORD0;
    [[vk::location(2)]] float3 tangent : TANGENT0;
    [[vk::location(3)]] float3 binormal : BINORMAL0;
    [[vk::location(4)]] float4 bone_weights : BLENDWEIGHT0;
    [[vk::location(5)]] float4 bone_indices : BLENDINDICES0;
};

struct Skeleton
{
    float4x4 bones[128];
};

cbuffer SkeletonBuffer : register(b2, space0)
{
    Skeleton skeleton;
}

float4x4 CreateSkinningMatrix(float4 bone_indices, float4 bone_weights)
{
    float4x4 skinning = 0;
    int4 indices = (int4)bone_indices;

    skinning += bone_weights.x * skeleton.bones[indices.x];
    skinning += bone_weights.y * skeleton.bones[indices.y];
    skinning += bone_weights.z * skeleton.bones[indices.z];
    skinning += bone_weights.w * skeleton.bones[indices.w];

    return skinning;
}

VSOutput main(VSInput input, uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.position = float4(input.position.xyz, 1.0);
    output.normal = input.normal;
    output.texcoord0 = input.texcoord0;
    output.tangent = input.tangent;
    output.binormal = input.binormal;
    output.bone_weights = input.bone_weights;
    output.bone_indices = input.bone_indices;

    return output;
}