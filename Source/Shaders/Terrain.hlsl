#include "include/Defines.hlsli"

PERMUTE(INSTANCING);
PERMUTE(SHADING_TYPE, DEFERRED);

#define TERRAIN_SPLAT_SCALE 0.002
#define TERRAIN_LAYER0_SCALE 0.08
#define TERRAIN_LAYER1_SCALE 0.045
#define TERRAIN_LAYER2_SCALE 0.06
#define TERRAIN_LAYER3_SCALE 0.10

#define TERRAIN_SLOPE_BLEND_START 0.25
#define TERRAIN_SLOPE_BLEND_END 0.55

#define TERRAIN_LAYER0_ROUGHNESS 0.90
#define TERRAIN_LAYER1_ROUGHNESS 0.85
#define TERRAIN_LAYER2_ROUGHNESS 0.85
#define TERRAIN_LAYER3_ROUGHNESS 0.60

struct PSInput
{
    float4 position_cs : SV_POSITION;
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord0 : TEXCOORD0;
    float2 texcoord1 : TEXCOORD1;
    float3 tangent : TANGENT;
    float3 bitangent : BINORMAL;
    float4 color : TEXCOORD2;
    nointerpolation float3 camera_position : TEXCOORD3;
    float4 position_ndc : TEXCOORD4;
    float4 previous_position_ndc : TEXCOORD5;
    nointerpolation uint object_index : TEXCOORD6;
    nointerpolation uint object_mask : TEXCOORD7;
};

struct PSOutput
{
    float4 gbuffer_albedo : SV_Target0;
    float4 gbuffer_normals : SV_Target1;
    uint gbuffer_material : SV_Target2;
    float2 gbuffer_velocity : SV_Target3;
};

DECLARE_SAMPLER(Default, SamplerLinear) SamplerState sampler_linear;
DECLARE_SAMPLER(Default, SamplerNearest) SamplerState sampler_nearest;

#define texture_sampler sampler_linear

#include "include/Material.hlsli"

#define MATERIAL_TEXTURE_TerrainSplatMap 6
#define MATERIAL_TEXTURE_TerrainLayer0 7
#define MATERIAL_TEXTURE_TerrainLayer1 8
#define MATERIAL_TEXTURE_TerrainLayer2 9
#define MATERIAL_TEXTURE_TerrainLayer3 10
#define MATERIAL_TEXTURE_TerrainNormal0 11
#define MATERIAL_TEXTURE_TerrainNormal1 12
#define MATERIAL_TEXTURE_TerrainNormal2 13
#define MATERIAL_TEXTURE_TerrainNormal3 14

#ifndef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(Material, TerrainSplatMap) Texture2D TerrainSplatMap;
DECLARE_SRV(Material, TerrainLayer0) Texture2D TerrainLayer0;
DECLARE_SRV(Material, TerrainLayer1) Texture2D TerrainLayer1;
DECLARE_SRV(Material, TerrainLayer2) Texture2D TerrainLayer2;
DECLARE_SRV(Material, TerrainLayer3) Texture2D TerrainLayer3;
DECLARE_SRV(Material, TerrainNormal0) Texture2D TerrainNormal0;
DECLARE_SRV(Material, TerrainNormal1) Texture2D TerrainNormal1;
DECLARE_SRV(Material, TerrainNormal2) Texture2D TerrainNormal2;
DECLARE_SRV(Material, TerrainNormal3) Texture2D TerrainNormal3;
#endif // !HYP_FEATURES_BINDLESS_TEXTURES

#include "include/Scene.hlsli"
#include "include/Packing.hlsli"
#include "include/EnvProbes.hlsli"
#include "include/Gbuffer.hlsli"
#include "include/Entity.hlsli"

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

float TerrainValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float4 SampleTriplanarScaled(Texture2D tex, float3 position, float3 normal, float scale)
{
    float3 blending = GetTriplanarBlend(normal);

    float4 sample_x = SAMPLE_TEXTURE_2D(texture_sampler, tex, position.zy * scale);
    float4 sample_y = SAMPLE_TEXTURE_2D(texture_sampler, tex, position.xz * scale);
    float4 sample_z = SAMPLE_TEXTURE_2D(texture_sampler, tex, position.xy * scale);

    return sample_x * blending.x + sample_y * blending.y + sample_z * blending.z;
}

float4 SampleTerrainLayer(uint layerIndex, float3 position, float3 normal)
{
    Texture2D tex;
    float scale;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainLayer0); scale = TERRAIN_LAYER0_SCALE; break;
    case 1: tex = GET_TEXTURE(material, TerrainLayer1); scale = TERRAIN_LAYER1_SCALE; break;
    case 2: tex = GET_TEXTURE(material, TerrainLayer2); scale = TERRAIN_LAYER2_SCALE; break;
    default: tex = GET_TEXTURE(material, TerrainLayer3); scale = TERRAIN_LAYER3_SCALE; break;
    }

    return SampleTriplanarScaled(tex, position, normal, scale);
}

void SampleTerrainLayerNormal(uint layerIndex, float3 position, float3 normal, out float3 tangentNormal)
{
    Texture2D tex;
    float scale;

    switch (layerIndex)
    {
    case 0: tex = GET_TEXTURE(material, TerrainNormal0); scale = TERRAIN_LAYER0_SCALE; break;
    case 1: tex = GET_TEXTURE(material, TerrainNormal1); scale = TERRAIN_LAYER1_SCALE; break;
    case 2: tex = GET_TEXTURE(material, TerrainNormal2); scale = TERRAIN_LAYER2_SCALE; break;
    default: tex = GET_TEXTURE(material, TerrainNormal3); scale = TERRAIN_LAYER3_SCALE; break;
    }

    float4 sample_value = SampleTriplanarScaled(tex, position, normal, scale);

    tangentNormal = sample_value.rgb * 2.0 - 1.0;
}

PSOutput PSMain(PSInput input)
{
    PSOutput output;

    float3x3 tbn_matrix = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));

    const float3 P = input.position.xyz;

    float3 N = normalize(input.normal);

    const float slope = saturate(1.0 - N.y);

    float4 weights;

    if (HAS_TEXTURE(material, TerrainSplatMap))
    {
        weights = SAMPLE_TEXTURE_2D(texture_sampler, GET_TEXTURE(material, TerrainSplatMap), P.xz * TERRAIN_SPLAT_SCALE);
    }
    else
    {
        const float threshold_jitter = (TerrainValueNoise(P.xz * 0.015) - 0.5) * 0.3;

        const float rock_blend = smoothstep(
            TERRAIN_SLOPE_BLEND_START + threshold_jitter,
            TERRAIN_SLOPE_BLEND_END + threshold_jitter,
            slope);

        weights = float4(1.0 - rock_blend, rock_blend, 0.0, 0.0);
    }
    
    weights.x *= float(HAS_TEXTURE(material, TerrainLayer0));
    weights.y *= float(HAS_TEXTURE(material, TerrainLayer1));
    weights.z *= float(HAS_TEXTURE(material, TerrainLayer2));
    weights.w *= float(HAS_TEXTURE(material, TerrainLayer3));

    const float total_weight = weights.x + weights.y + weights.z + weights.w;

    const bool any_layers = total_weight > 0.0001;

    if (any_layers)
    {
        weights /= total_weight;
    }

    float3 albedo = material.albedo.rgb;
    float roughness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_ROUGHNESS);
    const float metalness = GET_MATERIAL_PARAM(material, MATERIAL_PARAM_METALNESS);

    if (any_layers)
    {
        albedo = float3(0.0, 0.0, 0.0);
        roughness = 0.0;

        if (weights.x > 0.001)
        {
            albedo += weights.x * SampleTerrainLayer(0, P, N).rgb;
            roughness += weights.x * TERRAIN_LAYER0_ROUGHNESS;
        }

        if (weights.y > 0.001)
        {
            albedo += weights.y * SampleTerrainLayer(1, P, N).rgb;
            roughness += weights.y * TERRAIN_LAYER1_ROUGHNESS;
        }

        if (weights.z > 0.001)
        {
            albedo += weights.z * SampleTerrainLayer(2, P, N).rgb;
            roughness += weights.z * TERRAIN_LAYER2_ROUGHNESS;
        }

        if (weights.w > 0.001)
        {
            albedo += weights.w * SampleTerrainLayer(3, P, N).rgb;
            roughness += weights.w * TERRAIN_LAYER3_ROUGHNESS;
        }

        if (HAS_TEXTURE(material, TerrainNormal0)
            || HAS_TEXTURE(material, TerrainNormal1)
            || HAS_TEXTURE(material, TerrainNormal2)
            || HAS_TEXTURE(material, TerrainNormal3))
        {
            float3 layer_normal;
            float3 blended_normal = float3(0.0, 0.0, 0.0);

            if (weights.x > 0.001 && HAS_TEXTURE(material, TerrainNormal0))
            {
                SampleTerrainLayerNormal(0, P, N, layer_normal);
                blended_normal += weights.x * layer_normal;
            }

            if (weights.y > 0.001 && HAS_TEXTURE(material, TerrainNormal1))
            {
                SampleTerrainLayerNormal(1, P, N, layer_normal);
                blended_normal += weights.y * layer_normal;
            }

            if (weights.z > 0.001 && HAS_TEXTURE(material, TerrainNormal2))
            {
                SampleTerrainLayerNormal(2, P, N, layer_normal);
                blended_normal += weights.z * layer_normal;
            }

            if (weights.w > 0.001 && HAS_TEXTURE(material, TerrainNormal3))
            {
                SampleTerrainLayerNormal(3, P, N, layer_normal);
                blended_normal += weights.w * layer_normal;
            }

            if (abs(blended_normal.x) + abs(blended_normal.y) + abs(blended_normal.z) > 0.001)
            {
                N = normalize(mul(blended_normal, tbn_matrix));
            }
        }
    }

    output.gbuffer_albedo = float4(albedo, 1.0);

    roughness = roughness * roughness;

    float2 velocity = float2(
        ((input.position_ndc.xy / input.position_ndc.w) * 0.5 + 0.5)
            - ((input.previous_position_ndc.xy / input.previous_position_ndc.w) * 0.5 + 0.5));

    uint mask = input.object_mask;

    GBufferMaterialParams materialParams;
    materialParams.roughness = roughness;
    materialParams.metalness = metalness;
    materialParams.mask = mask;

    output.gbuffer_normals = GBufferPackNormal(N);

    float roughnessAndMetalPacked;
    uint maskPacked;
    GBufferPackMaterialParams(materialParams, roughnessAndMetalPacked, maskPacked);

    output.gbuffer_normals.x = roughnessAndMetalPacked;

    output.gbuffer_material = 0;

    // Mask is stored in the upper 4 bits of gbuffer_material
    output.gbuffer_material |= (maskPacked << 28u);

    output.gbuffer_velocity = velocity;

    return output;
}
