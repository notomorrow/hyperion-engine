#include "../Include/Defines.hlsli"
#include "../Include/Shared.hlsli"

PERMUTE(INSTANCING);
PERMUTE(SKINNING);
PERMUTE(TERRAIN_MORPH);

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../Include/Scene.hlsli"
#include "../Include/Entity.hlsli"
#include "../Include/TerrainMorph.hlsli"
#include "../Include/Material.hlsli"

#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#ifdef INSTANCING
DECLARE_SRV(Default, EntitiesBuffer) StructuredBuffer<Entity> entities;
DECLARE_SRV_DYNAMIC(Default, EntityInstanceBatchesBuffer) ByteAddressBuffer EntityInstanceBatchBuffer;
DECLARE_SRV(Default, InstanceDataBuffer) StructuredBuffer<InstanceTransform> InstanceTransforms;
#endif // INSTANCING

#include "../Include/Instancing.hlsli"

DECLARE_BUFFER_DYNAMIC(Default, CBuffer) cbuffer CBuffer
{
#ifndef INSTANCING
    Entity entity;
#else // INSTANCING
    Entity dummyEntity;
#endif // !INSTANCING
    Camera camera;
    Material material;
    float4x4 vpMatrix;
};

#ifdef VERTEX_SHADER

struct VSInput
{
    HYP_ATTRIBUTE float3 a_position : POSITION;
    HYP_ATTRIBUTE_OPTIONAL float3 a_normal : NORMAL;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord0 : TEXCOORD0;
    HYP_ATTRIBUTE_OPTIONAL float2 a_texcoord1 : TEXCOORD1;
    HYP_ATTRIBUTE_OPTIONAL uint a_bone_indices : BLENDINDICES;
    HYP_ATTRIBUTE_OPTIONAL float4 a_bone_weights : BLENDWEIGHT;
    HYP_ATTRIBUTE_OPTIONAL uint4 a_tree_limb_branch : TEXCOORD2;
    HYP_ATTRIBUTE_OPTIONAL uint2 a_tree_twig : TEXCOORD3;
    HYP_ATTRIBUTE_OPTIONAL uint2 a_foliage : TEXCOORD4;
};

struct VSOutput
{
    float4 position_cs : SV_POSITION;
};

#ifdef SKINNING
DECLARE_SRV_DYNAMIC(Default, SkeletonsBuffer) StructuredBuffer<float4x4> SkeletonsBuffer;
#include "../Include/Skinning.hlsli"
#endif // SKINNING

VSOutput VSMain(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

#ifdef INSTANCING
    uint entityIndex;
    uint dataOffset;
    LoadEntityIndexAndDataOffset(instanceId, entityIndex, dataOffset);

    Entity entity = entities[entityIndex];

    float4x4 model_matrix = mul(entity.model_matrix, LoadInstanceTransform(dataOffset));
#else
    float4x4 model_matrix = entity.model_matrix;
#endif

#if defined(TERRAIN_MORPH) && defined(VT_UV1)
    const float3 local_position = ApplyTerrainMorph(entity, model_matrix, input.a_position, input.a_texcoord1);
#else
    const float3 local_position = input.a_position;
#endif

#if defined(SKINNING) && defined(HYP_ATTRIBUTE_a_bone_indices) && defined(HYP_ATTRIBUTE_a_bone_weights)
    float4x4 skinning_matrix = CreateSkinningMatrix(input.a_bone_indices, input.a_bone_weights);
    float4 position = mul(model_matrix, mul(skinning_matrix, float4(local_position, 1.0)));
#else
    float4 position = mul(model_matrix, float4(local_position, 1.0));
#endif

    output.position_cs = mul(vpMatrix, position);

    return output;
}

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

struct PSInput
{
    float4 position_cs : SV_POSITION;
};

struct PSOutput
{
    float4 color : SV_Target0;
};

PSOutput PSMain(PSInput input)
{
    PSOutput output;
    output.color = float4(0.05, 0.42, 0.45, 1.0);

    return output;
}

#endif // PIXEL_SHADER
