/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

#include <Rendering/Clouds/CloudPass.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSHUpdate("Rendering/GPU/Glimmer/SHUpdate");
static EngineStatGpuTimer s_statGlimmerSHCompose("Rendering/GPU/Glimmer/SHCompose");
static EngineStatCounter<uint32> s_statGlimmerSHVoxelsTraced("Rendering/Glimmer/SHVoxelsTraced");

static StaticShaderPropertyId s_propSHModeUpdate { ShaderProperty(NAME("MODE"), NAME("UPDATE")) };
static StaticShaderPropertyId s_propSHModeCompose { ShaderProperty(NAME("MODE"), NAME("COMPOSE")) };

static constexpr int32 VoxelsPerFrame = 512;
static constexpr int32 RefreshVoxelsPerFrame = 256;

// lines the windows up with GlimmerSHOccupancy's, which snap to 8 of its half spaced voxels
static constexpr int32 WindowSnapVoxels = 4;

static constexpr size_t MaxPendingBoxes = 16;

static constexpr int32 SolidChangeMarginVoxels = 4;
static constexpr size_t MaxSolidChangeBoxes = 8;

static constexpr int32 RefreshSliceVoxels = int32(GlimmerSHGridXZ * GlimmerSHGridY) / 2;

static constexpr float RadianceHysteresis = 0.5f;

static constexpr uint32 RadianceSections = 3 + GlimmerLightEntries;
static constexpr uint32 RadianceColumns = 8;
static constexpr uint32 RadianceRows = (RadianceSections + RadianceColumns - 1) / RadianceColumns;

static constexpr uint32 MaxOccupancyWaitFrames = 8;

struct GlimmerSHUpdateConstants
{
    GlimmerSHVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
    Vec4i boxMin;      // xyz = absolute voxel of the dispatch's first voxel, w = cascade
    Vec4f rayRotation; // quaternion applied to this update's ray directions
    Vec4f params;      // x = foliage extinction, y = max ray distance, z = albedo where the ground's isn't known, w = how much of the radiance's history is kept
    GlimmerRelightShaderData relight;
    GlimmerProbeVolumeShaderData probes; // zeroed while the probes can't be sampled
    GlimmerSkyShaderData sky;
    EnvProbeShaderData skyProbe;
    GlimmerLightShaderData light;
};

GlimmerSHVolume::Box GlimmerSHVolume::Box::Intersect(const Box& a, const Box& b)
{
    Box result;
    result.min = Vec3i(MathUtil::Max(a.min.x, b.min.x), MathUtil::Max(a.min.y, b.min.y), MathUtil::Max(a.min.z, b.min.z));
    result.max = Vec3i(MathUtil::Min(a.max.x, b.max.x), MathUtil::Min(a.max.y, b.max.y), MathUtil::Min(a.max.z, b.max.z));

    if (result.IsEmpty())
    {
        result.max = result.min;
    }

    return result;
}

GlimmerSHVolume::Box GlimmerSHVolume::Box::Union(const Box& a, const Box& b)
{
    if (a.IsEmpty())
    {
        return b;
    }

    if (b.IsEmpty())
    {
        return a;
    }

    Box result;
    result.min = Vec3i(MathUtil::Min(a.min.x, b.min.x), MathUtil::Min(a.min.y, b.min.y), MathUtil::Min(a.min.z, b.min.z));
    result.max = Vec3i(MathUtil::Max(a.max.x, b.max.x), MathUtil::Max(a.max.y, b.max.y), MathUtil::Max(a.max.z, b.max.z));

    return result;
}

#pragma region GlimmerSHVolume

GlimmerSHVolume::GlimmerSHVolume()
    : m_refreshStep(0),
      m_refreshSlices {},
      m_updateIndex(0),
      m_seenTLASGeneration(~0u),
      m_occupancyWaitFrames(0),
      m_refreshBudget(0),
      m_composedLight {},
      m_hasComposedLight(false),
      m_shaderData {}
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
    }

    m_shaderData.info = Vec4u(GlimmerSHCascades, 0, 0, 0);
}

GlimmerSHVolume::~GlimmerSHVolume()
{
    m_dataTexture = Handle<Texture>();
    m_stateTexture = Handle<Texture>();
    m_radianceTexture = Handle<Texture>();
}

void GlimmerSHVolume::CreateResources()
{
    const Vec3u extent = Vec3u(GlimmerSHGridXZ, GlimmerSHGridY * GlimmerSHCascades, GlimmerSHGridXZ);

    m_dataTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA16F, extent, 1, NAME("GlimmerSHData"));
    m_stateTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA32, extent, 1, NAME("GlimmerSHState"));
    m_radianceTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA16F, Vec3u(extent.x * RadianceColumns, extent.y * RadianceRows, extent.z), 1, NAME("GlimmerSHRadiance"));
}

const GpuImageViewRef& GlimmerSHVolume::GetDataImageView() const
{
    return m_dataTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_dataTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetStateImageView() const
{
    return m_stateTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_stateTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetRadianceImageView() const
{
    return m_radianceTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_radianceTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

GlimmerSHVolume::Box GlimmerSHVolume::GetWindow(const Vec3i& origin)
{
    return Box { origin, origin + Vec3i(int32(GlimmerSHGridXZ), int32(GlimmerSHGridY), int32(GlimmerSHGridXZ)) };
}

void GlimmerSHVolume::MoveWindow(uint32 cascadeIndex, const Vec3i& origin)
{
    Cascade& cascade = m_cascades[cascadeIndex];

    if (cascade.origin.HasValue() && *cascade.origin == origin)
    {
        return;
    }

    const Box newWindow = GetWindow(origin);
    const Vec3i size = newWindow.Extent();

    const bool isJump = !cascade.origin.HasValue()
        || MathUtil::Abs(origin.x - cascade.origin->x) >= size.x
        || MathUtil::Abs(origin.y - cascade.origin->y) >= size.y
        || MathUtil::Abs(origin.z - cascade.origin->z) >= size.z;

    if (isJump)
    {
        cascade.pending.Clear();
        cascade.pending.PushBack(newWindow);
    }
    else
    {
        const Box oldWindow = GetWindow(*cascade.origin);

        for (Box& pendingBox : cascade.pending)
        {
            pendingBox = Box::Intersect(pendingBox, newWindow);
        }

        // what scrolled in, as up to three slabs: exposed x over the whole window, then exposed y and z over what's left
        const Box kept = Box::Intersect(oldWindow, newWindow);

        Box remaining = newWindow;

        for (int32 axis = 0; axis < 3; axis++)
        {
            Box slab = remaining;

            if (origin[axis] > (*cascade.origin)[axis])
            {
                slab.min[axis] = MathUtil::Max(kept.max[axis], newWindow.min[axis]);
            }
            else
            {
                slab.max[axis] = MathUtil::Min(kept.min[axis], newWindow.max[axis]);
            }

            if (!slab.IsEmpty())
            {
                cascade.pending.PushBack(slab);
            }

            remaining.min[axis] = kept.min[axis];
            remaining.max[axis] = kept.max[axis];
        }
    }

    cascade.origin = origin;

    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin.x, origin.y, origin.z, 1);
}

void GlimmerSHVolume::AddPending(uint32 cascadeIndex, const Box& box)
{
    Cascade& cascade = m_cascades[cascadeIndex];

    if (!cascade.origin.HasValue())
    {
        return;
    }

    const Box window = GetWindow(*cascade.origin);
    const Box clipped = Box::Intersect(box, window);

    if (clipped.IsEmpty())
    {
        return;
    }

    if (cascade.pending.Size() >= MaxPendingBoxes)
    {
        // joins the pending box it adds the fewest voxels to
        size_t bestIndex = 0;
        int64 bestGrowth = INT64_MAX;

        for (size_t pendingIndex = 0; pendingIndex < cascade.pending.Size(); pendingIndex++)
        {
            const Box& pendingBox = cascade.pending[pendingIndex];
            const int64 growth = Box::Union(pendingBox, clipped).Volume() - pendingBox.Volume();

            if (growth < bestGrowth)
            {
                bestGrowth = growth;
                bestIndex = pendingIndex;
            }
        }

        cascade.pending[bestIndex] = Box::Union(cascade.pending[bestIndex], clipped);

        return;
    }

    cascade.pending.PushBack(clipped);
}

void GlimmerSHVolume::AddPendingWorld(const Vec3f& worldMin, const Vec3f& worldMax, int32 marginVoxels)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float invSpacing = 1.0f / GetGlimmerSHCascadeSpacing(cascadeIndex);

        AddPending(cascadeIndex, Box {
            Vec3i(
                int32(MathUtil::Floor(worldMin.x * invSpacing)) - marginVoxels,
                int32(MathUtil::Floor(worldMin.y * invSpacing)) - marginVoxels,
                int32(MathUtil::Floor(worldMin.z * invSpacing)) - marginVoxels),
            Vec3i(
                int32(MathUtil::Floor(worldMax.x * invSpacing)) + 1 + marginVoxels,
                int32(MathUtil::Floor(worldMax.y * invSpacing)) + 1 + marginVoxels,
                int32(MathUtil::Floor(worldMax.z * invSpacing)) + 1 + marginVoxels) });
    }
}

void GlimmerSHVolume::DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, float radianceHysteresis, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers)
{
    CommandRecorder& cr = frame->cr;

    if (!inOutHasBarriers)
    {
        inOutHasBarriers = true;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(s_propSHModeUpdate);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHUpdate"), shaderProperties));
    }

    GlimmerSHUpdateConstants constants {};
    constants.volume = m_shaderData;
    constants.ground = inputs.surfaceCache->GetGroundShaderData();
    constants.spans = inputs.spanCache->GetShaderData();
    constants.occupancy = *inputs.occupancy;
    constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, int32(cascadeIndex));
    constants.rayRotation = MakeGlimmerRandomRotation(m_updateIndex++);
    constants.params = Vec4f(
        GetGlimmerFoliageExtinction(),
        GlimmerMaxRayDistance,
        MathUtil::Clamp(g_cvGlimmerGroundAlbedo.Get(), 0.0f, 1.0f),
        radianceHysteresis);

    if (inputs.relight && inputs.relightImageView.IsValid())
    {
        constants.relight = *inputs.relight;
    }

    const GlimmerSWRTProbeVolume* probeVolume = inputs.probeVolume;

    if (probeVolume && probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get())
    {
        constants.probes = probeVolume->GetShaderData();
    }

    GetGlimmerSkyShaderData(inputs.skyProbe, constants.sky, constants.skyProbe);

    constants.light = inputs.light;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);

    if (inputs.cloudPass != nullptr)
    {
        inputs.cloudPass->WriteShaderData(*RI.cbufferAllocator);
    }
    else
    {
        static const EffectVolumeShaderData s_noCloudVolume {};
        static const CloudWeatherMapShaderData s_noWeatherMap {};
        static const CloudShadowMapShaderData s_noShadowMap {};

        RI.cbufferAllocator->Write(&s_noCloudVolume);
        RI.cbufferAllocator->Write(&s_noWeatherMap);
        RI.cbufferAllocator->Write(&s_noShadowMap);
    }

    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, inputs.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "OutData"_sh, RI.textureViewCache->GetOrCreate(m_dataTexture));
    cr << SetShaderUniform(uniformIndex++, "OutState"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
    cr << SetShaderUniform(uniformIndex++, "OutRadiance"_sh, RI.textureViewCache->GetOrCreate(m_radianceTexture));
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
    cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

    if (probeVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeLightBuffer"_sh, probeVolume->GetLightBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        // HACK: any structured buffer will do, as the zeroed constants keep the probes from being read
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeLightBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, inputs.occupancyMaskBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

    if (inputs.relightImageView.IsValid())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerRelightTexture"_sh, inputs.relightImageView);
    }

    const Vec3i extent = box.Extent();

    cr << DispatchCompute(Vec3u { uint32(extent.x), uint32(extent.y), uint32(extent.z) });

    s_statGlimmerSHVoxelsTraced += uint32(extent.x * extent.y * extent.z);
}

bool GlimmerSHVolume::DispatchRefreshSlice(
    Frame* frame,
    uint32 cascadeIndex,
    int32& inOutSlice,
    float radianceHysteresis,
    bool& outFinishedCascade,
    const GlimmerSHVolumeUpdateInputs& inputs,
    bool& inOutHasBarriers)
{
    outFinishedCascade = false;

    Cascade& cascade = m_cascades[cascadeIndex];

    if (!cascade.origin.HasValue() || cascade.pending.Any())
    {
        return false;
    }

    const Box window = GetWindow(*cascade.origin);

    const int32 firstSlice = window.min.z * 2;
    const int32 endSlice = window.max.z * 2;

    // slices that scrolled out are skipped; the ones that scrolled in were traced as they did
    const int32 sliceIndex = MathUtil::Max(inOutSlice, firstSlice);

    // the rest of the pass scrolled out
    if (sliceIndex >= endSlice)
    {
        inOutSlice = firstSlice;
        outFinishedCascade = true;

        return false;
    }

    Box slice = window;
    slice.min.z = sliceIndex >> 1;
    slice.max.z = slice.min.z + 1;

    if ((sliceIndex & 1) == 0)
    {
        slice.max.y = slice.min.y + int32(GlimmerSHGridY / 2);
    }
    else
    {
        slice.min.y += int32(GlimmerSHGridY / 2);
    }

    DispatchBox(frame, cascadeIndex, slice, radianceHysteresis, inputs, inOutHasBarriers);

    inOutSlice = sliceIndex + 1;

    if (inOutSlice >= endSlice)
    {
        inOutSlice = firstSlice;
        outFinishedCascade = true;
    }

    return true;
}

void GlimmerSHVolume::Compose(Frame* frame, const GlimmerLightShaderData& light)
{
    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHCompose);

    CommandRecorder& cr = frame->cr;

    GlimmerSHUpdateConstants constants {};
    constants.light = light;

    static const EffectVolumeShaderData s_noCloudVolume {};
    static const CloudWeatherMapShaderData s_noWeatherMap {};
    static const CloudShadowMapShaderData s_noShadowMap {};

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Write(&s_noCloudVolume);
    RI.cbufferAllocator->Write(&s_noWeatherMap);
    RI.cbufferAllocator->Write(&s_noShadowMap);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propSHModeCompose);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHUpdate"), shaderProperties));

    cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(1, "OutRadiance"_sh, RI.textureViewCache->GetOrCreate(m_radianceTexture));

    // 2 rows of voxels a group
    cr << DispatchCompute(Vec3u { 1, GlimmerSHGridY * GlimmerSHCascades / 2, GlimmerSHGridXZ });

    cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_composedLight = light;
    m_hasComposedLight = true;
}

void GlimmerSHVolume::Update(Frame* frame, const GlimmerSHVolumeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.spanCache->GetSpansBuffer().IsValid() || !inputs.occupancy || !inputs.occupancyMaskBuffer.IsValid())
    {
        return;
    }

    if (!m_dataTexture.IsValid())
    {
        CreateResources();
    }

    // voxels that aren't traced this frame still hold the light as it was put together before
    if (m_shaderData.info.w != 0 && (!m_hasComposedLight || memcmp(&m_composedLight, &inputs.light, sizeof(GlimmerLightShaderData)) != 0))
    {
        Compose(frame, inputs.light);
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params.z = MathUtil::Clamp(g_cvGlimmerVisibility.Get(), 0.0f, 1.0f);

        const Vec3i origin = Vec3i(
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.x / spacing)) - int32(GlimmerSHGridXZ / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.y / spacing)) - int32(GlimmerSHGridY / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.z / spacing)) - int32(GlimmerSHGridXZ / 2), WindowSnapVoxels));

        MoveWindow(cascadeIndex, origin);
    }

    if (inputs.tlas && inputs.tlas->IsReady() && inputs.tlas->GetGeneration() != m_seenTLASGeneration)
    {
        GlimmerSceneChanges changes;

        const bool isEverywhere = m_seenTLASGeneration == ~0u || !inputs.tlas->GetChangesSince(m_seenTLASGeneration, changes);

        if (isEverywhere)
        {
            for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
            {
                AddPending(cascadeIndex, GetWindow(*m_cascades[cascadeIndex].origin));
            }
        }
        else
        {
            if (g_cvGlimmerSkipLodOnlyChanges.Get())
            {
                GlimmerSceneChanges worldChanges;

                for (const GlimmerSceneChange& change : changes)
                {
                    if (!change.isLodOnly)
                    {
                        worldChanges.Add(change);
                    }
                }

                changes = std::move(worldChanges);
            }

            changes.Quantize(MaxSolidChangeBoxes);

            for (const GlimmerSceneChange& change : changes)
            {
                AddPendingWorld(change.bounds.min, change.bounds.max, SolidChangeMarginVoxels);
            }
        }

        m_seenTLASGeneration = inputs.tlas->GetGeneration();
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const GlimmerTexelRect& uploadedRect = inputs.surfaceCache->GetUploadedRect(levelIndex);

        if (uploadedRect.IsEmpty())
        {
            continue;
        }

        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

        AddPendingWorld(
            Vec3f(float(uploadedRect.min.x) * texelSize, -1e6f, float(uploadedRect.min.y) * texelSize),
            Vec3f(float(uploadedRect.max.x) * texelSize, 1e6f, float(uploadedRect.max.y) * texelSize),
            1);
    }

    // a viewer outrunning the occupancy's one rebuild a frame can't stall the far field for good
    if (inputs.isOccupancySettled)
    {
        m_occupancyWaitFrames = 0;
    }
    else if (m_occupancyWaitFrames < MaxOccupancyWaitFrames)
    {
        m_occupancyWaitFrames++;

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHUpdate);

    int32 budget = VoxelsPerFrame;
    bool hasBarriers = false;

    // finest first: it's what the camera sits in
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades && budget > 0; cascadeIndex++)
    {
        Cascade& cascade = m_cascades[cascadeIndex];

        while (cascade.pending.Any() && budget > 0)
        {
            Box& box = cascade.pending.Front();

            if (box.IsEmpty())
            {
                cascade.pending.PopFront();

                continue;
            }

            const Vec3i extent = box.Extent();
            const int32 slices = MathUtil::Clamp(budget / MathUtil::Max(extent.x * extent.y, 1), 1, extent.z);

            Box chunk = box;
            chunk.max.z = chunk.min.z + slices;

            DispatchBox(frame, cascadeIndex, chunk, RadianceHysteresis, inputs, hasBarriers);

            budget -= extent.x * extent.y * slices;
            box.min.z += slices;
        }
    }

    if (g_cvGlimmerRunOnChange.Get())
    {
        const int32 rate = MathUtil::Max(g_cvGlimmerSHRefreshVoxels.Get(), 0);

        m_refreshBudget = MathUtil::Min(m_refreshBudget + rate, MathUtil::Max(rate, RefreshSliceVoxels) * 2);

        for (uint32 attempt = 0; attempt < 2 * GlimmerSHCascades && m_refreshBudget >= RefreshSliceVoxels; attempt++)
        {
            uint32 ruler = ++m_refreshStep;
            uint32 cascadeIndex = 0;

            while ((ruler & 1u) == 0u && cascadeIndex + 1 < GlimmerSHCascades)
            {
                ruler >>= 1;
                cascadeIndex++;
            }

            bool hasFinishedCascade = false;

            if (!DispatchRefreshSlice(frame, cascadeIndex, m_refreshSlices[cascadeIndex], RadianceHysteresis, hasFinishedCascade, inputs, hasBarriers))
            {
                continue;
            }

            m_refreshBudget -= RefreshSliceVoxels;

            attempt = 0;
        }

        budget = 0;
    }

    const int32 sliceVoxels = RefreshSliceVoxels;

    budget = MathUtil::Min(budget, RefreshVoxelsPerFrame);

    for (uint32 attempt = 0; attempt < 2 * GlimmerSHCascades && budget >= sliceVoxels; attempt++)
    {
        uint32 ruler = ++m_refreshStep;
        uint32 cascadeIndex = 0;

        while ((ruler & 1u) == 0u && cascadeIndex + 1 < GlimmerSHCascades)
        {
            ruler >>= 1;
            cascadeIndex++;
        }

        bool hasFinishedCascade = false;

        if (!DispatchRefreshSlice(frame, cascadeIndex, m_refreshSlices[cascadeIndex], RadianceHysteresis, hasFinishedCascade, inputs, hasBarriers))
        {
            continue;
        }

        budget -= sliceVoxels;

        attempt = 0;
    }

    if (hasBarriers)
    {
        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::ShaderResource);

        m_shaderData.info.w = 1;
    }
}

#pragma endregion GlimmerSHVolume

} // namespace Hyperion
