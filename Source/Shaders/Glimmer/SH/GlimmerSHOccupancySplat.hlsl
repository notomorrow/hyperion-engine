#include "../../Include/Defines.hlsli"
#include "../../Include/Shared.hlsli"
#include "../../Include/Aabb.hlsli"
#include "../../Include/Lightmap.hlsli"

#define HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS
#include "../../Include/Material.hlsli"
#undef HYP_DO_NOT_DEFINE_DESCRIPTOR_SETS

#include "../GlimmerCommon.hlsli"

#define GLIMMER_SH_OCCUPANCY_NO_TRACE
#include "GlimmerSHOccupancy.hlsli"

PERMUTE(MODE, CLEAR, SPLAT, RESOLVE, BLOCKS)

struct GlimmerSHOccupancySplatConstants
{
    /// xyz = absolute voxel of the cascade's window
    /// w = cascade
    int4 origin;
    
    /// x = voxel spacing
    /// y = 1 / spacing
    float4 params;
    
    /// x = span instances
    /// y = span chunks
    /// z = groups along x
    uint4 counts;
    
    /// xyz = absolute voxel. only voxels in the box are cleared and splatted
    int4 boxMin;
    
    /// xyz = absolute voxel, exclusive
    int4 boxMax;

    uint4 lightmapStencils;
};

DECLARE_BUFFER_DYNAMIC(GlimmerSHOccupancySplat, CBuffer) cbuffer CBuffer
{
    GlimmerSHOccupancySplatConstants constants;
};

DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancy) RWTexture3D<float4> OutOccupancy;
DECLARE_UAV(GlimmerSHOccupancySplat, OutOccupancyMask) RWStructuredBuffer<uint> OutOccupancyMask;
DECLARE_UAV(GlimmerSHOccupancySplat, OutAlbedoSums) RWStructuredBuffer<uint> OutAlbedoSums;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanInstancesBuffer) StructuredBuffer<GlimmerSpanInstance> spanInstances;
DECLARE_SRV(GlimmerSHOccupancySplat, SpanChunksBuffer) StructuredBuffer<GlimmerSpanChunk> spanChunks;
DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerTrianglesBuffer) StructuredBuffer<uint> glimmerTriangles;
DECLARE_SRV(GlimmerSHOccupancySplat, MaterialsBuffer) StructuredBuffer<Material> materials;

#ifdef HYP_FEATURES_BINDLESS_TEXTURES
DECLARE_SRV(BindlessResources0, Textures) Texture2D textures[];
#endif

DECLARE_SAMPLER(GlimmerSHOccupancySplat, SamplerLinearMipmap) SamplerState glimmerMaterialSampler;

DECLARE_SRV(GlimmerSHOccupancySplat, GlimmerTriangleLightmapUVsBuffer) StructuredBuffer<uint> glimmerTriangleLightmapUVs;

DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture0) Texture2D LightmapVolumeIrradianceTexture0;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture1) Texture2D LightmapVolumeIrradianceTexture1;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture2) Texture2D LightmapVolumeIrradianceTexture2;
DECLARE_SRV(GlimmerSHOccupancySplat, LightmapVolumeIrradianceTexture3) Texture2D LightmapVolumeIrradianceTexture3;

#include "../GlimmerMaterial.hlsli"

#define GROUP_SIZE 64

#define ALBEDO_SUM_WORDS 4u
#define ALBEDO_WEIGHT_SCALE 1024.0

// per face : weighted irradiance (3 values), weight
#define LIGHTMAP_SUM_WORDS (GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES * 4u)
#define VOXEL_SUM_WORDS (ALBEDO_SUM_WORDS + LIGHTMAP_SUM_WORDS)
#define LIGHTMAP_MAX_IRRADIANCE 32.0
#define LIGHTMAP_IRRADIANCE_SCALE 16.0

uint AlbedoSumIndex(int3 voxel)
{
    const uint3 texel = GlimmerSHOccupancyTexel(0u, voxel);

    return ((texel.z * uint(GLIMMER_SH_OCCUPANCY_GRID_Y) + texel.y) * uint(GLIMMER_SH_OCCUPANCY_GRID_XZ) + texel.x) * VOXEL_SUM_WORDS;
}

uint LightmapSumIndex(int3 voxel)
{
    return AlbedoSumIndex(voxel) + ALBEDO_SUM_WORDS;
}

float2 LoadLightmapUV(uint uvBase, uint corner)
{
    const uint packed = glimmerTriangleLightmapUVs[uvBase + corner];

    return float2(packed & 0xFFFFu, packed >> 16u) / 65535.0;
}

float3 SampleLightmapPage(uint page, float2 atlasUV)
{
    float3 irradiance;

    if (page == 0u)
    {
        irradiance = LightmapVolumeIrradianceTexture0.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else if (page == 1u)
    {
        irradiance = LightmapVolumeIrradianceTexture1.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else if (page == 2u)
    {
        irradiance = LightmapVolumeIrradianceTexture2.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }
    else
    {
        irradiance = LightmapVolumeIrradianceTexture3.SampleLevel(glimmerMaterialSampler, atlasUV, 0.0).rgb;
    }

    return min(max(irradiance, (float3)0.0), (float3)LIGHTMAP_MAX_IRRADIANCE);
}

#define BACK_BIAS 0.01

float3 TransformPoint(GlimmerSpanInstance instance, float3 position)
{
    const float4 homogeneous = float4(position, 1.0);

    return float3(dot(instance.objectToWorld0, homogeneous), dot(instance.objectToWorld1, homogeneous), dot(instance.objectToWorld2, homogeneous));
}

struct TriangleLightmap
{
    bool isValid;
    
    uint page;
    
    uint rectOffset;
    uint rectSize;
    
    float2 uv0;
    
    float2 uvEdge1;
    float2 uvEdge2;

    float3 p0;
    float3 edge1;
    float3 edge2;
};

TriangleLightmap LoadTriangleLightmap(GlimmerSpanInstance instance, uint localTriangle, uint cascadeIndex, float3 p0, float3 p1, float3 p2)
{
    TriangleLightmap lightmap = (TriangleLightmap)0;
    lightmap.rectOffset = instance.lightmap.y;
    lightmap.rectSize = instance.lightmap.z;
    lightmap.p0 = p0;
    lightmap.edge1 = p1 - p0;
    lightmap.edge2 = p2 - p0;

    if (cascadeIndex >= uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES) || instance.lightmap.x == 0xFFFFFFFFu)
    {
        return lightmap;
    }

    const uint stencilValue = GetLightmapStencilValue(lightmap.rectOffset);

    [unroll]
    for (uint page = 0; page < 4; page++)
    {
        if (!lightmap.isValid && stencilValue != 0u && constants.lightmapStencils[page] == stencilValue)
        {
            lightmap.isValid = true;
            lightmap.page = page;
        }
    }

    if (lightmap.isValid)
    {
        const uint uvBase = (instance.lightmap.x + localTriangle) * 3u;

        lightmap.uv0 = LoadLightmapUV(uvBase, 0u);
        lightmap.uvEdge1 = LoadLightmapUV(uvBase, 1u) - lightmap.uv0;
        lightmap.uvEdge2 = LoadLightmapUV(uvBase, 2u) - lightmap.uv0;
    }

    return lightmap;
}

// the baked irradiance at the point of the triangle nearest position
float3 SampleTriangleLightmap(TriangleLightmap lightmap, float3 position)
{
    const float edge11 = dot(lightmap.edge1, lightmap.edge1);
    const float edge12 = dot(lightmap.edge1, lightmap.edge2);
    const float edge22 = dot(lightmap.edge2, lightmap.edge2);

    const float3 toPosition = position - lightmap.p0;
    const float toPosition1 = dot(toPosition, lightmap.edge1);
    const float toPosition2 = dot(toPosition, lightmap.edge2);

    float2 barycentrics = float2(
        edge22 * toPosition1 - edge12 * toPosition2,
        edge11 * toPosition2 - edge12 * toPosition1) / max(edge11 * edge22 - edge12 * edge12,
        1e-20);
    
    barycentrics = max(barycentrics, (float2)0.0);
    barycentrics /= max(barycentrics.x + barycentrics.y, 1.0);

    const float2 uv = lightmap.uv0 + lightmap.uvEdge1 * barycentrics.x + lightmap.uvEdge2 * barycentrics.y;

    return SampleLightmapPage(lightmap.page, GetLightmapAtlasUV(lightmap.rectOffset, lightmap.rectSize, uv));
}

void SplatLightmapIrradiance(int3 solidVoxel, float3 irradiance, float coveredArea, float3 facingNormal, bool isDoubleSided)
{
    // texels nothing was baked into are black
    if (!any(irradiance > 0.0))
    {
        return;
    }

    const uint lightmapSumIndex = LightmapSumIndex(solidVoxel);

    [unroll]
    for (uint axis = 0; axis < 3; axis++)
    {
        const float facing = GetComponent(facingNormal, axis);
        const uint faceWeight = uint(coveredArea * facing * facing * ALBEDO_WEIGHT_SCALE + 0.5);
        const uint3 weightedIrradiance = uint3(irradiance * (float(faceWeight) * LIGHTMAP_IRRADIANCE_SCALE) + 0.5);

        [unroll]
        for (uint side = 0; side < 2; side++)
        {
            if (faceWeight != 0u && (isDoubleSided || (facing < 0.0) == (side == 1u)))
            {
                const uint faceIndex = lightmapSumIndex + (axis * 2u + side) * 4u;

                uint previous;
                InterlockedAdd(OutAlbedoSums[faceIndex + 0u], weightedIrradiance.r, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 1u], weightedIrradiance.g, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 2u], weightedIrradiance.b, previous);
                InterlockedAdd(OutAlbedoSums[faceIndex + 3u], faceWeight, previous);
            }
        }
    }
}

// what a triangle's columns need, set up once per lane that splats it
struct SplatSetup
{
    bool isValid;
    float3 p0;
    float3 p1;
    float3 p2;
    float3 normal;
    float3 facingNormal;
    bool isDoubleSided;
    float spacing;
    float invSpacing;
    int3 voxelMin;
    int3 voxelMax;
    float coveredArea;
    uint weight;
    uint3 weightedAlbedo;
    uint axisU;
    uint axisV;
    uint axisW;
    float3 plane; // p0 along u, v and w
    float triangleMinW;
    float triangleMaxW;
    TriangleLightmap lightmap;
};

SplatSetup SetupSplatTriangle(GlimmerSpanInstance instance, uint localTriangle, uint cascadeIndex)
{
    SplatSetup setup = (SplatSetup)0;

    const float4 albedoAlpha = GlimmerGetMaterialAverageAlbedoAlpha(instance.data.z);

    // mostly cut out alpha tested cards (chains, grilles) let most light through
    if ((instance.data.w & GLIMMER_INSTANCE_FLAG_ALPHA_TESTED) != 0u && albedoAlpha.a < 0.5)
    {
        return setup;
    }

    const GlimmerTriangle glimmerTriangle = LOAD_GLIMMER_TRIANGLE(glimmerTriangles, instance.data.x + localTriangle);

    float3 p0 = TransformPoint(instance, glimmerTriangle.position0);
    float3 p1 = TransformPoint(instance, glimmerTriangle.position1);
    float3 p2 = TransformPoint(instance, glimmerTriangle.position2);

    const float3 normal = cross(p1 - p0, p2 - p0);

    if (dot(normal, normal) < 1e-12)
    {
        return setup;
    }

    setup.spacing = constants.params.x / float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS);
    setup.invSpacing = constants.params.y * float(GLIMMER_SH_OCCUPANCY_SUB_VOXELS);

    setup.isDoubleSided = (instance.data.w & GLIMMER_INSTANCE_FLAG_DOUBLE_SIDED) != 0u;
    setup.facingNormal = normalize(normal) * ((instance.data.w & GLIMMER_INSTANCE_FLAG_MIRRORED) != 0u ? -1.0 : 1.0);

    setup.lightmap = LoadTriangleLightmap(instance, localTriangle, cascadeIndex, p0, p1, p2);

    if (!setup.isDoubleSided)
    {
        const float3 behind = setup.facingNormal * (-BACK_BIAS * setup.spacing);

        p0 += behind;
        p1 += behind;
        p2 += behind;
    }

    const int3 windowMin = constants.origin.xyz;
    const int3 boxMin = max(windowMin, constants.boxMin.xyz) * GLIMMER_SH_OCCUPANCY_SUB_VOXELS;
    const int3 boxMax = min(windowMin + GlimmerSHOccupancyGridSize, constants.boxMax.xyz) * GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1;

    setup.voxelMin = max(int3(floor(min(p0, min(p1, p2)) * setup.invSpacing)), boxMin);
    setup.voxelMax = min(int3(floor(max(p0, max(p1, p2)) * setup.invSpacing)), boxMax);

    if (any(setup.voxelMax < setup.voxelMin))
    {
        return setup;
    }

    setup.isValid = true;
    setup.p0 = p0;
    setup.p1 = p1;
    setup.p2 = p2;
    setup.normal = normal;

    setup.coveredArea = min(0.5 * length(normal) * setup.invSpacing * setup.invSpacing, 1.0);
    setup.weight = max(uint(setup.coveredArea * ALBEDO_WEIGHT_SCALE + 0.5), 1u);
    setup.weightedAlbedo = uint3(saturate(albedoAlpha.rgb) * float(setup.weight) + 0.5);

    const float3 absNormal = abs(normal);
    setup.axisW = select(absNormal.x >= absNormal.y, select(absNormal.x >= absNormal.z, 0u, 2u), select(absNormal.y >= absNormal.z, 1u, 2u));
    setup.axisU = (setup.axisW + 1u) % 3u;
    setup.axisV = (setup.axisW + 2u) % 3u;

    setup.plane = float3(GetComponent(p0, setup.axisU), GetComponent(p0, setup.axisV), GetComponent(p0, setup.axisW));

    setup.triangleMinW = GetComponent(min(p0, min(p1, p2)), setup.axisW);
    setup.triangleMaxW = GetComponent(max(p0, max(p1, p2)), setup.axisW);

    return setup;
}

// the columns across the triangle's dominant axis that its box covers
uint SplatColumnCount(SplatSetup setup)
{
    if (!setup.isValid)
    {
        return 0u;
    }

    const int columnsU = GetComponent(setup.voxelMax, setup.axisU) - GetComponent(setup.voxelMin, setup.axisU) + 1;
    const int columnsV = GetComponent(setup.voxelMax, setup.axisV) - GetComponent(setup.voxelMin, setup.axisV) + 1;

    return uint(columnsU * columnsV);
}

void SplatColumn(SplatSetup setup, uint columnIndex, uint cascadeIndex)
{
    const int minU = GetComponent(setup.voxelMin, setup.axisU);
    const int columnsU = GetComponent(setup.voxelMax, setup.axisU) - minU + 1;

    const int u = minU + int(columnIndex % uint(columnsU));
    const int v = GetComponent(setup.voxelMin, setup.axisV) + int(columnIndex / uint(columnsU));

    const float normalU = GetComponent(setup.normal, setup.axisU);
    const float normalV = GetComponent(setup.normal, setup.axisV);
    const float normalW = GetComponent(setup.normal, setup.axisW);

    float columnMinW = 1e30;
    float columnMaxW = -1e30;

    [unroll]
    for (uint corner = 0; corner < 4; corner++)
    {
        const float cornerU = float(u + int(corner & 1u)) * setup.spacing;
        const float cornerV = float(v + int(corner >> 1)) * setup.spacing;
        const float cornerW = setup.plane.z - (normalU * (cornerU - setup.plane.x) + normalV * (cornerV - setup.plane.y)) / normalW;

        columnMinW = min(columnMinW, cornerW);
        columnMaxW = max(columnMaxW, cornerW);
    }

    const int firstW = max(int(floor(max(columnMinW, setup.triangleMinW) * setup.invSpacing)), GetComponent(setup.voxelMin, setup.axisW));
    const int lastW = min(int(floor(min(columnMaxW, setup.triangleMaxW) * setup.invSpacing)), GetComponent(setup.voxelMax, setup.axisW));

    const float3 halfExtent = (float3)(0.5 * setup.spacing);

    for (int w = firstW; w <= lastW; w++)
    {
        int coords[3];
        coords[setup.axisU] = u;
        coords[setup.axisV] = v;
        coords[setup.axisW] = w;

        const int3 voxel = int3(coords[0], coords[1], coords[2]);
        const float3 center = (float3(voxel) + 0.5) * setup.spacing;

        if (!TriangleOverlapsBox(setup.p0, setup.p1, setup.p2, setup.normal, center, halfExtent))
        {
            continue;
        }

        const int3 solidVoxel = voxel >> GLIMMER_SH_OCCUPANCY_SUB_SHIFT;
        const uint bit = GlimmerSHOccupancySubBit(voxel & (GLIMMER_SH_OCCUPANCY_SUB_VOXELS - 1));

        uint previous;
        InterlockedOr(OutOccupancyMask[GlimmerSHOccupancyMaskIndex(cascadeIndex, solidVoxel) + (bit >> 5)], 1u << (bit & 31u), previous);

        const uint sumIndex = AlbedoSumIndex(solidVoxel);

        InterlockedAdd(OutAlbedoSums[sumIndex + 0u], setup.weightedAlbedo.r, previous);
        InterlockedAdd(OutAlbedoSums[sumIndex + 1u], setup.weightedAlbedo.g, previous);
        InterlockedAdd(OutAlbedoSums[sumIndex + 2u], setup.weightedAlbedo.b, previous);
        InterlockedAdd(OutAlbedoSums[sumIndex + 3u], setup.weight, previous);

        if (setup.lightmap.isValid)
        {
            SplatLightmapIrradiance(solidVoxel, SampleTriangleLightmap(setup.lightmap, center), setup.coveredArea, setup.facingNormal, setup.isDoubleSided);
        }
    }
}

// a triangle with more columns than this is shared out across the group rather than left to one lane
#define SPLAT_LANE_COLUMNS 16u

groupshared uint gsBigTriangles[GLIMMER_SPAN_CHUNK_TRIANGLES];
groupshared uint gsNumBigTriangles;

[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    const uint threadIndex = (groupId.y * constants.counts.z + groupId.x) * GROUP_SIZE + groupIndex;
    const uint cascadeIndex = uint(constants.origin.w);

#if defined(MODE_CLEAR) || defined(MODE_RESOLVE)
    const uint3 boxExtent = uint3(constants.boxMax.xyz - constants.boxMin.xyz);

    if (threadIndex >= boxExtent.x * boxExtent.y * boxExtent.z)
    {
        return;
    }

    const int3 voxel = constants.boxMin.xyz + int3(
        threadIndex % boxExtent.x,
        (threadIndex / boxExtent.x) % boxExtent.y,
        threadIndex / (boxExtent.x * boxExtent.y));

    const uint maskIndex = GlimmerSHOccupancyMaskIndex(cascadeIndex, voxel);
    const uint sumIndex = AlbedoSumIndex(voxel);

#if defined(MODE_RESOLVE)
    if ((OutOccupancyMask[maskIndex] | OutOccupancyMask[maskIndex + 1u]) != 0u)
    {
        const float3 albedoSum = float3(OutAlbedoSums[sumIndex + 0u], OutAlbedoSums[sumIndex + 1u], OutAlbedoSums[sumIndex + 2u]);

        OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel)] = float4(albedoSum / float(max(OutAlbedoSums[sumIndex + 3u], 1u)), 1.0);

        if (cascadeIndex < uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES))
        {
            const uint lightmapSumIndex = LightmapSumIndex(voxel);
            const uint lightmapIndex = GlimmerSHOccupancyLightmapIndex(cascadeIndex, voxel);

            [unroll]
            for (uint face = 0; face < GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES; face++)
            {
                const uint faceIndex = lightmapSumIndex + face * 4u;
                const uint faceWeight = OutAlbedoSums[faceIndex + 3u];

                if (faceWeight != 0u)
                {
                    const float3 irradianceSum = float3(OutAlbedoSums[faceIndex + 0u], OutAlbedoSums[faceIndex + 1u], OutAlbedoSums[faceIndex + 2u]);

                    OutOccupancyMask[lightmapIndex + face] = PackRGB9E5(irradianceSum / (float(faceWeight) * LIGHTMAP_IRRADIANCE_SCALE));
                }
            }
        }
    }
#else
    OutOccupancy[GlimmerSHOccupancyTexel(cascadeIndex, voxel)] = (float4)0.0;

    [unroll]
    for (uint sumWord = 0; sumWord < ALBEDO_SUM_WORDS; sumWord++)
    {
        OutAlbedoSums[sumIndex + sumWord] = 0u;
    }

    [unroll]
    for (uint wordIndex = 0; wordIndex < GLIMMER_SH_OCCUPANCY_MASK_WORDS; wordIndex++)
    {
        OutOccupancyMask[maskIndex + wordIndex] = 0u;
    }

    const uint lightmapSumIndex = LightmapSumIndex(voxel);

    [unroll]
    for (uint lightmapSumWord = 0; lightmapSumWord < LIGHTMAP_SUM_WORDS; lightmapSumWord++)
    {
        OutAlbedoSums[lightmapSumIndex + lightmapSumWord] = 0u;
    }

    if (cascadeIndex < uint(GLIMMER_SH_OCCUPANCY_TRACED_CASCADES))
    {
        const uint lightmapIndex = GlimmerSHOccupancyLightmapIndex(cascadeIndex, voxel);

        [unroll]
        for (uint face = 0; face < GLIMMER_SH_OCCUPANCY_LIGHTMAP_FACES; face++)
        {
            OutOccupancyMask[lightmapIndex + face] = 0u;
        }
    }
#endif
#elif defined(MODE_BLOCKS)
    const int3 blockMin = constants.boxMin.xyz >> GLIMMER_SH_OCCUPANCY_BLOCK_SHIFT;
    const int3 blockMax = (constants.boxMax.xyz + (GLIMMER_SH_OCCUPANCY_BLOCK_VOXELS - 1)) >> GLIMMER_SH_OCCUPANCY_BLOCK_SHIFT;
    const uint3 blockExtent = uint3(blockMax - blockMin);

    if (threadIndex >= blockExtent.x * blockExtent.y * blockExtent.z)
    {
        return;
    }

    const int3 block = blockMin + int3(threadIndex % blockExtent.x, (threadIndex / blockExtent.x) % blockExtent.y, threadIndex / (blockExtent.x * blockExtent.y));
    const int3 firstVoxel = block << GLIMMER_SH_OCCUPANCY_BLOCK_SHIFT;

    uint solid = 0u;

    for (uint inner = 0; inner < uint(GLIMMER_SH_OCCUPANCY_BLOCK_VOXELS * GLIMMER_SH_OCCUPANCY_BLOCK_VOXELS * GLIMMER_SH_OCCUPANCY_BLOCK_VOXELS); inner++)
    {
        const uint side = uint(GLIMMER_SH_OCCUPANCY_BLOCK_VOXELS - 1);
        const int3 voxel = firstVoxel + int3(inner & side, (inner >> GLIMMER_SH_OCCUPANCY_BLOCK_SHIFT) & side, inner >> (2 * GLIMMER_SH_OCCUPANCY_BLOCK_SHIFT));
        const uint maskIndex = GlimmerSHOccupancyMaskIndex(cascadeIndex, voxel);

        solid |= OutOccupancyMask[maskIndex] | OutOccupancyMask[maskIndex + 1u];
    }

    OutOccupancyMask[GlimmerSHOccupancyBlockIndex(cascadeIndex, firstVoxel)] = solid != 0u ? 1u : 0u;
#elif defined(MODE_SPLAT)
    const uint chunkIndex = groupId.y * constants.counts.z + groupId.x;

    if (groupIndex == 0u)
    {
        gsNumBigTriangles = 0u;
    }

    GroupMemoryBarrierWithGroupSync();

    // every lane reaches the barriers, so a chunk with nothing to splat only sets this
    bool isActive = chunkIndex < constants.counts.y;

    GlimmerSpanChunk chunk = (GlimmerSpanChunk)0;
    GlimmerSpanInstance instance = (GlimmerSpanInstance)0;

    if (isActive)
    {
        chunk = spanChunks[chunkIndex];

        // the whole chunk goes when its instance misses the box
        const float3 windowMinPosition = float3(max(constants.origin.xyz, constants.boxMin.xyz)) * constants.params.x;
        const float3 windowMaxPosition = float3(min(constants.origin.xyz + GlimmerSHOccupancyGridSize, constants.boxMax.xyz)) * constants.params.x;

        isActive = !(any(chunk.boundsMax.xyz < windowMinPosition) || any(chunk.boundsMin.xyz > windowMaxPosition));
    }

    if (isActive)
    {
        instance = spanInstances[asuint(chunk.boundsMin.w)];

        // leaves are the canopy spans' business.
        // they dim light rather than stop it.
        isActive = (instance.data.w & GLIMMER_INSTANCE_FLAG_FOLIAGE) == 0u;
    }

    if (isActive)
    {
        for (uint chunkTriangle = groupIndex; chunkTriangle < GLIMMER_SPAN_CHUNK_TRIANGLES; chunkTriangle += GROUP_SIZE)
        {
            const uint localTriangle = asuint(chunk.boundsMax.w) + chunkTriangle;

            if (localTriangle >= instance.data.y)
            {
                break;
            }

            const SplatSetup setup = SetupSplatTriangle(instance, localTriangle, cascadeIndex);
            const uint numColumns = SplatColumnCount(setup);

            if (numColumns > SPLAT_LANE_COLUMNS)
            {
                uint slot;
                InterlockedAdd(gsNumBigTriangles, 1u, slot);

                gsBigTriangles[slot] = localTriangle;

                continue;
            }

            for (uint column = 0; column < numColumns; column++)
            {
                SplatColumn(setup, column, cascadeIndex);
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // the big ones, a column a lane
    const uint numBigTriangles = gsNumBigTriangles;

    for (uint bigIndex = 0; bigIndex < numBigTriangles; bigIndex++)
    {
        const SplatSetup setup = SetupSplatTriangle(instance, gsBigTriangles[bigIndex], cascadeIndex);
        const uint numColumns = SplatColumnCount(setup);

        for (uint column = groupIndex; column < numColumns; column += GROUP_SIZE)
        {
            SplatColumn(setup, column, cascadeIndex);
        }
    }
#endif
}
