/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeDebug.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
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

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerSWRTDebug("Rendering/GPU/Glimmer/SWRTDebug");

static EngineStatCounter<uint32> s_statGlimmerSWRTDebugRays("Rendering/Glimmer/SWRTDebugRays");

static EngineStatCounter<uint32> s_statGlimmerLightingChanges("Rendering/Glimmer/LightingChanges", false);

static constexpr float RegionRecenterFraction = 0.25f;

// how far the sky's L1 has to move, relative to its mean, before the caches are woken for it
static constexpr float SkyShapeChange = 0.1f;

// how long the sun has to hold still before the key ahead is put on it, how long that key gets to be lit,
// and how long the light then takes to move onto it
static constexpr double SunRestSeconds = 3.0;
static constexpr double SunKeySettleSeconds = 6.0;
static constexpr float SunKeyRampSeconds = 1.5f;

struct GlimmerSWRTDebugConstants
{
    Vec4u dimensionsModeInstances;
    GlimmerFootprintMaskShaderData mask;
    Vec4f params;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
};

#pragma region GlimmerTechnique

GlimmerSceneRegionParams GlimmerTechnique::GetSceneRegionParams()
{
    const float nearFieldRadius = GetGlimmerNearFieldRadius();

    GlimmerSceneRegionParams params;
    params.radius = MathUtil::Max(nearFieldRadius, g_cvGlimmerSWRTSpansRadius.Get());
    params.recenterDistance = nearFieldRadius * RegionRecenterFraction;
    params.snap = GlimmerFootprintMaskCellSize;

    return params;
}

void GlimmerTechnique::WriteApplyShaderData(CBufferAllocator& cbufferAllocator, const GlimmerTechnique* technique)
{
    GlimmerProbeVolumeShaderData probeShaderData {};
    GlimmerSHVolumeShaderData shShaderData {};

    if (technique)
    {
        if (technique->m_probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get())
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
    const GlimmerSWRTProbeVolume* probeVolume = technique ? technique->m_probeVolume.Get() : nullptr;
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
    : m_footprintMask(MakeUnique<GlimmerFootprintMask>()),
      m_probeVolume(MakeUnique<GlimmerSWRTProbeVolume>()),
      m_probeDebug(MakeUnique<GlimmerSWRTProbeDebug>()),
      m_shOccupancy(MakeUnique<GlimmerSHOccupancy>()),
      m_shVolume(MakeUnique<GlimmerSHVolume>()),
      m_relight(MakeUnique<GlimmerRelight>()),
      m_maskGeneration(~0u),
      m_sunKeys {},
      m_sunKeyFrom(0),
      m_sunKeyWeight(0.0f),
      m_hasSunKeys(false),
      m_hasSunPath(false),
      m_isSunKeySettling(false),
      m_lastSunMoveSeconds(0.0),
      m_sunKeySettleSeconds(0.0),
      m_sunKeyUpdateSeconds(0.0),
      m_hasLitSignature(false)
{
}

static float AngleBetween(const Vec3f& a, const Vec3f& b)
{
    return MathUtil::Arccos(MathUtil::Clamp(a.Dot(b), -1.0f, 1.0f));
}

// axis has to be at a right angle to the direction
static Vec3f TurnAbout(const Vec3f& direction, const Vec3f& axis, float angle)
{
    return (direction * MathUtil::Cos(angle) + axis.Cross(direction) * MathUtil::Sin(angle)).Normalized();
}

bool GlimmerTechnique::UpdateSunKeys(const Vec3f& sunDirection)
{
    const double nowSeconds = double(Time::Now().ToMilliseconds()) * 0.001;
    const float deltaSeconds = float(MathUtil::Clamp(nowSeconds - m_sunKeyUpdateSeconds, 0.0, 1.0));

    m_sunKeyUpdateSeconds = nowSeconds;

    const float tolerance = MathUtil::DegToRad(MathUtil::Clamp(g_cvGlimmerSunAngleThreshold.Get(), 0.01f, 45.0f));
    const float spacing = MathUtil::Max(MathUtil::DegToRad(MathUtil::Clamp(g_cvGlimmerSunKeySpacing.Get(), 0.1f, 45.0f)), tolerance * 2.0f);

    bool hasRetargeted = false;

    const auto putKeysOnSun = [&]()
    {
        for (Vec3f& direction : m_sunKeys.directions)
        {
            direction = sunDirection;
        }

        m_sunKeyWeight = 0.0f;
        m_hasSunKeys = true;
        m_hasSunPath = false;
        m_isSunKeySettling = false;

        hasRetargeted = true;
    };

    if (!m_hasSunKeys)
    {
        m_lastSunDirection = sunDirection;
        m_lastSunMoveSeconds = nowSeconds;

        putKeysOnSun();
    }
    else if (GlimmerSunKeys == 1)
    {
        if (AngleBetween(m_sunKeys.directions[0], sunDirection) > tolerance)
        {
            putKeysOnSun();
        }
    }
    else
    {
        if (AngleBetween(sunDirection, m_lastSunDirection) > 1e-5f)
        {
            m_lastSunDirection = sunDirection;
            m_lastSunMoveSeconds = nowSeconds;
        }

        uint32 toKey = (m_sunKeyFrom + 1) % GlimmerSunKeys;

        if (m_isSunKeySettling)
        {
            if (AngleBetween(m_sunKeys.directions[toKey], sunDirection) > tolerance)
            {
                putKeysOnSun();
            }
            else if (nowSeconds - m_sunKeySettleSeconds > SunKeySettleSeconds)
            {
                m_sunKeyWeight = MathUtil::Min(m_sunKeyWeight + deltaSeconds / SunKeyRampSeconds, 1.0f);

                if (m_sunKeyWeight >= 1.0f)
                {
                    m_sunKeys.directions[m_sunKeyFrom] = m_sunKeys.directions[toKey];
                    m_sunKeyFrom = toKey;
                    m_sunKeyWeight = 0.0f;
                    m_hasSunPath = false;
                    m_isSunKeySettling = false;
                }
            }
        }
        else if (!m_hasSunPath)
        {
            const Vec3f fromDirection = m_sunKeys.directions[m_sunKeyFrom];
            const float moved = AngleBetween(fromDirection, sunDirection);

            if (moved > spacing)
            {
                putKeysOnSun();
            }
            else if (moved > tolerance)
            {
                // the sun has set off: the other key goes ahead of it
                m_sunKeys.directions[toKey] = TurnAbout(fromDirection, fromDirection.Cross(sunDirection).Normalized(), spacing);
                m_hasSunPath = true;

                hasRetargeted = true;
            }
        }

        if (m_hasSunPath && !m_isSunKeySettling)
        {
            const Vec3f fromDirection = m_sunKeys.directions[m_sunKeyFrom];
            const Vec3f toDirection = m_sunKeys.directions[toKey];

            const Vec3f axis = fromDirection.Cross(toDirection).Normalized();
            const Vec3f onPath = (sunDirection - axis * sunDirection.Dot(axis)).Normalized();

            float span = AngleBetween(fromDirection, toDirection);
            float along = AngleBetween(fromDirection, onPath) * (fromDirection.Cross(onPath).Dot(axis) < 0.0f ? -1.0f : 1.0f);

            const float offPath = MathUtil::Abs(MathUtil::Arcsin(MathUtil::Clamp(sunDirection.Dot(axis), -1.0f, 1.0f)));

            if (offPath > tolerance || along < -tolerance || along > span + spacing)
            {
                putKeysOnSun();
            }
            else
            {
                if (along >= span)
                {
                    // the sun passed the key ahead: the one behind goes ahead of that one
                    m_sunKeys.directions[m_sunKeyFrom] = TurnAbout(toDirection, axis, spacing);
                    m_sunKeyFrom = toKey;
                    toKey = (m_sunKeyFrom + 1) % GlimmerSunKeys;

                    along -= span;
                    span = spacing;

                    hasRetargeted = true;
                }

                const float t = MathUtil::Clamp(along / MathUtil::Max(span, 1e-6f), 0.0f, 1.0f);

                // eased, as the key ahead is still being lit when the sun sets off for it
                m_sunKeyWeight = t * t * (3.0f - 2.0f * t);

                if (nowSeconds - m_lastSunMoveSeconds > SunRestSeconds
                    && m_sunKeyWeight > 1e-3f
                    && AngleBetween(m_sunKeys.directions[toKey], sunDirection) > 1e-4f)
                {
                    // the sun stopped between its keys: the key ahead is put on it, and the light follows once that key is lit
                    m_sunKeys.directions[toKey] = sunDirection;
                    m_isSunKeySettling = true;
                    m_sunKeySettleSeconds = nowSeconds;

                    hasRetargeted = true;
                }
            }
        }
    }

    for (float& weight : m_sunKeys.weights)
    {
        weight = 0.0f;
    }

    m_sunKeys.weights[m_sunKeyFrom] = 1.0f - m_sunKeyWeight;
    m_sunKeys.weights[(m_sunKeyFrom + 1) % GlimmerSunKeys] += m_sunKeyWeight;

    return hasRetargeted;
}

EnumFlags<GlimmerLightingChangeFlags> GlimmerTechnique::UpdateLightingSignature(EnvProbe* skyProbe)
{
    const WorldShaderData* worldData = GetWorldBufferData();

    if (!worldData)
    {
        return GlimmerLightingChangeFlags::None;
    }

    const Vec3f toSun = worldData->sunDirectionIntensity.GetXYZ();
    const Vec3f sunDirection = toSun.LengthSquared() > 1e-8f ? toSun.Normalized() : Vec3f(0.0f, 1.0f, 0.0f);

    Vec3f skyShape[3];
    GetGlimmerSkyShape(skyProbe, skyShape);

    EnumFlags<GlimmerLightingChangeFlags> changes = GlimmerLightingChangeFlags::None;

    if (UpdateSunKeys(sunDirection))
    {
        changes |= GlimmerLightingChangeFlags::SunDirection;
    }

    float skyShapeChange = 0.0f;

    for (uint32 term = 0; term < 3; term++)
    {
        const Vec3f difference = skyShape[term] - m_litSkyShape[term];

        skyShapeChange = MathUtil::Max(skyShapeChange, MathUtil::Max(MathUtil::Abs(difference.x), MathUtil::Max(MathUtil::Abs(difference.y), MathUtil::Abs(difference.z))));
    }

    // with the sky split into its SH terms, its shape is in what the caches hold
    if (GlimmerSkyTerms == 1 && (!m_hasLitSignature || skyShapeChange > SkyShapeChange))
    {
        changes |= GlimmerLightingChangeFlags::SkyShape;
    }

    if (!changes)
    {
        return GlimmerLightingChangeFlags::None;
    }

    if (g_cvGlimmerSWRTProbesLogStats.Get())
    {
        HYP_LOG(Rendering, Info, "Glimmer lighting changed: sun keys {} and {} deg from the sun, weight ahead {}, on a path {}, settling {}, sky shape moved {}",
            MathUtil::RadToDeg(AngleBetween(m_sunKeys.directions[0], sunDirection)),
            MathUtil::RadToDeg(AngleBetween(m_sunKeys.directions[GlimmerSunKeys - 1], sunDirection)),
            m_sunKeyWeight, m_hasSunPath, m_isSunKeySettling, skyShapeChange);
    }

    for (uint32 term = 0; term < 3; term++)
    {
        m_litSkyShape[term] = skyShape[term];
    }

    m_hasLitSignature = true;

    s_statGlimmerLightingChanges++;

    return changes;
}

GlimmerTechnique::~GlimmerTechnique()
{
}

BoundingBox GlimmerTechnique::GetTracedRegion(const BoundingBox& sceneRegion) const
{
    const Vec3f regionCenter = sceneRegion.GetCenter();
    const float swrtRadius = GetGlimmerSWRTRadius(sceneRegion);

    BoundingBox swrtRegion = sceneRegion;
    swrtRegion.min.x = regionCenter.x - swrtRadius;
    swrtRegion.max.x = regionCenter.x + swrtRadius;
    swrtRegion.min.z = regionCenter.z - swrtRadius;
    swrtRegion.max.z = regionCenter.z + swrtRadius;

    return swrtRegion;
}

bool GlimmerTechnique::IsReady() const
{
    return (m_probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get()) || m_shVolume->IsReady();
}

void GlimmerTechnique::Update(const GlimmerTechniqueUpdateContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const GlimmerTLAS& tlas = *context.tlas;

    if (tlas.IsReady() && (context.tlasSwapped || tlas.GetGeneration() != m_maskGeneration))
    {
        const BoundingBox& gatherRegion = tlas.GetActiveRegion();
        const Vec3f regionCenter = gatherRegion.GetCenter();

        m_footprintMask->Rebuild(context.frame, tlas, Vec2f(regionCenter.x, regionCenter.z), GetGlimmerSWRTRadius(gatherRegion));

        m_maskGeneration = tlas.GetGeneration();
    }

    if (context.updateLighting && context.surfaceCache && context.spanCache)
    {
        const bool probesEnabled = g_cvGlimmerSWRTProbesEnabled.Get();

        const EnumFlags<GlimmerLightingChangeFlags> lightingChanges = UpdateLightingSignature(context.skyProbe);

        GlimmerLightShaderData light;
        GetGlimmerLightShaderData(context.skyProbe, m_sunKeys, light);
        const bool isRelightSettled = !probesEnabled
            || (!m_relight->IsBurstRelightingSun() && !(lightingChanges & GlimmerLightingChangeFlags::SunDirection));

        if (probesEnabled)
        {
            m_relight->Schedule(*context.channelState, *context.surfaceCache, *context.spanCache, lightingChanges);
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

        m_shOccupancy->Update(context.frame, context.channelState->viewerPosition, tlas, *context.blasCache, lightmapPages);

        GlimmerSHVolumeUpdateInputs shInputs;
        shInputs.viewerPosition = context.channelState->viewerPosition;
        shInputs.surfaceCache = context.surfaceCache;
        shInputs.spanCache = context.spanCache;
        shInputs.occupancy = &m_shOccupancy->GetShaderData();
        shInputs.occupancyImageView = m_shOccupancy->GetImageView();
        shInputs.occupancyMaskBuffer = m_shOccupancy->GetMaskBuffer();
        shInputs.isOccupancySettled = m_shOccupancy->IsSettled();
        shInputs.relight = &m_relight->GetShaderData();
        shInputs.tlas = context.tlas;
        shInputs.relightImageView = relightImageView;
        shInputs.isRelightSettled = isRelightSettled;
        shInputs.probeVolume = m_probeVolume.Get();
        shInputs.skyProbe = context.skyProbe;
        shInputs.cloudPass = context.cloudPass;
        shInputs.lightingChanges = lightingChanges;
        shInputs.light = light;

        m_shVolume->Update(context.frame, shInputs);

        // near field
        GlimmerSWRTProbeUpdateInputs probeInputs;
        probeInputs.occupancy = &m_shOccupancy->GetShaderData();
        probeInputs.occupancyImageView = m_shOccupancy->GetImageView();
        probeInputs.viewerPosition = context.channelState->viewerPosition;
        probeInputs.surfaceCache = context.surfaceCache;
        probeInputs.spanCache = context.spanCache;
        probeInputs.blasCache = context.blasCache;
        probeInputs.tlas = context.tlas;
        probeInputs.footprintMask = m_footprintMask.Get();
        probeInputs.shVolume = m_shVolume.Get();
        probeInputs.skyProbe = context.skyProbe;
        probeInputs.relight = m_relight.Get();
        probeInputs.cloudPass = context.cloudPass;
        probeInputs.lightingChanges = lightingChanges;
        probeInputs.light = light;
        probeInputs.isLightingSettling = m_relight->IsBursting() || m_shVolume->IsBursting();

        if (probesEnabled)
        {
            m_probeVolume->Update(context.frame, probeInputs);
        }
    }

    if (context.updateLighting && context.channel && context.surfaceCache && g_cvGlimmerSWRTProbesEnabled.Get()
        && (g_cvGlimmerSWRTDebugProbes.Get() > int(GlimmerSWRTDebugProbes::None) || g_cvGlimmerSWRTProbesLogStats.Get()))
    {
        m_probeDebug->Update(context.frame, *m_probeVolume, *context.surfaceCache, *m_shOccupancy, *context.channel);
    }
    else
    {
        m_probeDebug->Reset();
    }
}

bool GlimmerTechnique::RenderDebugView(const GlimmerDebugViewContext& context)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (context.techniqueView <= int(GlimmerSWRTDebugView::None) || context.techniqueView >= int(GlimmerSWRTDebugView::Max))
    {
        return false;
    }

    if (!context.blasCache || !context.tlas || !context.tlas->IsReady() || !context.blasCache->IsReady() || !context.surfaceCache || !context.spanCache)
    {
        return false;
    }

    if (!m_shOccupancy->GetMaskBuffer().IsValid())
    {
        return false;
    }

    const GlimmerTLAS& tlas = *context.tlas;
    const GlimmerBLASCache& blasCache = *context.blasCache;

    View* view = context.view;
    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSWRTDebug);

    GlimmerSWRTDebugConstants constants {};
    constants.dimensionsModeInstances = Vec4u(extent.x, extent.y, uint32(context.techniqueView), tlas.GetNumInstances());
    constants.mask = m_footprintMask->GetShaderData();
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

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, tlas.GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerInstancesBuffer"_sh, tlas.GetInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceShaderData)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, blasCache.GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBLASNode)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "FootprintMaskBuffer"_sh, m_footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, context.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, context.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, context.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, context.surfaceCache->GetGroundAlbedoImageView());

    // the occupancy's zeroed constants (before it's built) have every cascade read as missing
    const GpuImageViewRef& occupancyImageView = m_shOccupancy->GetImageView();
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, occupancyImageView.IsValid() ? occupancyImageView : RI.placeholderData->GetImageView3D1x1x1R8());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, m_shOccupancy->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    s_statGlimmerSWRTDebugRays += extent.x * extent.y;

    return true;
}

#pragma region GlimmerTechnique

} // namespace Hyperion
