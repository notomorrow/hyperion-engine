/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeDebug.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerChannel.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerHeightfieldDebug("Rendering/GPU/Glimmer/HeightfieldDebug");

static EngineStatCounter<uint32> s_statGlimmerHeightfieldDebugRays("Rendering/Glimmer/HeightfieldDebugRays");

static constexpr float RegionRecenterFraction = 0.25f;

struct GlimmerHeightfieldDebugConstants
{
    Vec4u dimensionsMode;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
};

#pragma region GlimmerTechnique

GlimmerSceneRegionParams GlimmerTechnique::GetSceneRegionParams()
{
    const float minRadius = GetGlimmerRegionMinRadius();

    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(minRadius, g_cvGlimmerSpansRadius.Get());
    params.recenterDistance = minRadius * RegionRecenterFraction;

    return params;
}

void GlimmerTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    GlimmerProbeVolumeShaderData probeShaderData {};
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique)
    {
        if (technique->m_probeVolume->IsReady() && g_cvGlimmerProbesEnabled.Get())
        {
            probeShaderData = technique->m_probeVolume->GetShaderData();
        }

        if (technique->m_shVolume->IsReady())
        {
            shShaderData = technique->m_shVolume->GetShaderData();
        }
    }

    cbufferAllocator.Write(&probeShaderData);
    cbufferAllocator.Write(&shShaderData);
}

uint32 GlimmerTechnique::BindApplyResources(CommandRecorder& cr, uint32 uniformIndex, const GlimmerTechnique* technique)
{
    const GlimmerProbeVolume* probeVolume = technique ? technique->m_probeVolume.Get() : nullptr;
    const GlimmerSHVolume* shVolume = technique ? technique->m_shVolume.Get() : nullptr;

    if (probeVolume)
    {
        // placeholders until the volume has buffers
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume->GetSHBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        // HACK: any structured buffer will do. as the zeroed constants keep lighting from reading it anyways
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }

    if (shVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, shVolume->GetDataImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, shVolume->GetStateImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, shVolume->GetRadianceImageView());
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
    }

    return uniformIndex;
}

GlimmerTechnique::GlimmerTechnique()
    : m_probeVolume(MakeUnique<GlimmerProbeVolume>()),
      m_probeDebug(MakeUnique<GlimmerProbeDebug>()),
      m_shOccupancy(MakeUnique<GlimmerSHOccupancy>()),
      m_shVolume(MakeUnique<GlimmerSHVolume>()),
      m_relight(MakeUnique<GlimmerRelight>()),
      m_brickWorld(MakeUnique<GlimmerBrickWorld>())
{
}

GlimmerTechnique::~GlimmerTechnique()
{
    EnqueueDeletion(std::move(m_scratchBuffer));
}

bool GlimmerTechnique::IsReady() const
{
    return (m_probeVolume->IsReady() && g_cvGlimmerProbesEnabled.Get()) || m_shVolume->IsReady();
}

void GlimmerTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerSceneInstances& sceneInstances = *context.sceneInstances;

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        if (!m_scratchBuffer.IsValid())
        {
            m_scratchBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, 16, alignof(Vec4f));
            Check(m_scratchBuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
            m_scratchBuffer->SetDebugName(NAME("GlimmerScratch"));
#endif
        }

        const bool probesEnabled = g_cvGlimmerProbesEnabled.Get();

        GlimmerLightShaderData light;
        GetGlimmerLightShaderData(context.skyProbe, light);

        if (probesEnabled)
        {
            m_relight->Schedule(*context.channelState, *context.surfaceCache, *context.spanCache);
        }
        else
        {
            m_relight->Invalidate();
        }

        const GpuImageViewRef& relightImageView = m_relight->GetImageView();

        context.frame->cr << InsertBarrier(m_relight->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        // far field
        GlimmerLightmapPages lightmapPages;

        if (context.sceneProxies)
        {
            CollectGlimmerLightmapPages(*context.sceneProxies, lightmapPages);
        }

        m_shOccupancy->Update(context.frame, context.channelState->viewerPosition, context.channelState->lookAhead, sceneInstances, *context.triangleCache, lightmapPages, *m_scratchBuffer);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.lookAhead = context.channelState->lookAhead;
        shInputs.budgetScale = context.channelState->budgetScale;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();
        shInputs.occupancyMaskBuffer = m_shOccupancy->GetMaskBuffer();
        shInputs.isOccupancySettled = m_shOccupancy->IsSettled();
        shInputs.relight = &m_relight->GetShaderData();
        shInputs.sceneInstances = context.sceneInstances;
        shInputs.relightImageView = relightImageView;
        shInputs.probeVolume = m_probeVolume.Get();
        shInputs.skyProbe = context.skyProbe;
        shInputs.cloudPass = context.cloudPass;
        shInputs.light = light;

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerProbeUpdateInputs probeInputs;
        probeInputs.occupancy = &m_shOccupancy->GetShaderData();
        probeInputs.occupancyImageView = m_shOccupancy->GetImageView();
        probeInputs.viewerPosition = context.channelState->viewerPosition;
        probeInputs.lookAhead = context.channelState->lookAhead;
        probeInputs.budgetScale = context.channelState->budgetScale;
        probeInputs.surfaceCache = context.surfaceCache;
        probeInputs.spanCache = context.spanCache;
        probeInputs.triangleCache = context.triangleCache;
        probeInputs.sceneInstances = context.sceneInstances;
        probeInputs.shVolume = m_shVolume.Get();
        probeInputs.skyProbe = context.skyProbe;
        probeInputs.relight = m_relight.Get();
        probeInputs.brickWorld = g_cvGlimmerBricksEnabled.Get() ? m_brickWorld.Get() : nullptr;
        probeInputs.cloudPass = context.cloudPass;
        probeInputs.light = light;
        probeInputs.scratchBuffer = m_scratchBuffer.Get();

        if (probesEnabled)
        {
            m_probeVolume->Update(context.frame, probeInputs);
        }
    }

    if (context.updateLighting && context.channel && context.surfaceCache && g_cvGlimmerProbesEnabled.Get()
        && (g_cvGlimmerProbesDebug.Get() > int(GlimmerProbeDebugMode::None) || g_cvGlimmerProbesLogStats.Get()))
    {
        m_probeDebug->Update(context.frame, *m_probeVolume, *context.surfaceCache, *m_shOccupancy, *context.channel);
    }
    else
    {
        m_probeDebug->Reset();
    }

    if (g_cvGlimmerBricksEnabled.Get() && context.triangleCache)
    {
        const bool hasViewer = context.channelState && context.channelState->hasViewer;

        const Vec3f viewerPosition = hasViewer ? context.channelState->viewerPosition : context.region.GetCenter();
        const Vec3f lookAhead = hasViewer ? context.channelState->lookAhead : Vec3f::Zero();
        const float budgetScale = context.channelState ? context.channelState->budgetScale : 1.0f;

        GlimmerLightShaderData light;
        GetGlimmerLightShaderData(context.skyProbe, light);

        m_brickWorld->Update(context.frame, viewerPosition, lookAhead, budgetScale, sceneInstances, *context.triangleCache, this, light);
    }
}

bool GlimmerTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (context.techniqueView >= int(GlimmerHeightfieldDebugView::Max))
    {
        const int brickView = context.techniqueView - int(GlimmerHeightfieldDebugView::Max) + 1;

        return brickView < int(GlimmerBrickDebugView::Max) && m_brickWorld->RenderDebugView(context, GlimmerBrickDebugView(brickView));
    }

    if (context.techniqueView <= int(GlimmerHeightfieldDebugView::None))
    {
        return false;
    }

    if (!context.triangleCache || !context.sceneInstances || !context.sceneInstances->IsReady() || !context.triangleCache->IsReady() || !context.surfaceCache || !context.spanCache)
    {
        return false;
    }

    if (!m_shOccupancy->GetMaskBuffer().IsValid())
    {
        return false;
    }

    View* view = context.view;
    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerHeightfieldDebug);

    GlimmerHeightfieldDebugConstants constants {};
    constants.dimensionsMode = Vec4u(extent.x, extent.y, uint32(context.techniqueView), 0);
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.ground = context.surfaceCache->GetGroundShaderData();
    constants.spans = context.spanCache->GetShaderData();
    constants.occupancy = m_shOccupancy->GetShaderData();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    GpuImage* depthImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage();
    GpuImage* normalsImage = gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage();

    cr << InsertBarrier(depthImage, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(normalsImage, ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerHeightfieldDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    // the occupancy's zeroed constants (before it's built) have every cascade read as missing
    const GpuImageViewRef& occupancyImageView = m_shOccupancy->GetImageView();
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancyImageView.IsValid() ? occupancyImageView : RI.placeholderData->GetImageView3D1x1x1R8());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, m_shOccupancy->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    s_statGlimmerHeightfieldDebugRays += extent.x * extent.y;

    return true;
}

#pragma region GlimmerTechnique

} // namespace Hyperion
