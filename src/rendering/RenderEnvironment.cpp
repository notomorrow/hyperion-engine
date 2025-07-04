/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <system/AppContext.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypClass.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <util/MeshBuilder.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

RenderEnvironment::RenderEnvironment()
    : m_ddgi(DDGIInfo {
          .aabb = { { -45.0f, -5.0f, -45.0f }, { 45.0f, 60.0f, 45.0f } } }),
      m_hasRtRadiance(false),
      m_hasDdgiProbes(false),
      m_rtInitialized(false)
{
}

RenderEnvironment::~RenderEnvironment()
{
    m_particleSystem.Reset();

    m_gaussianSplatting.Reset();

    if (m_hasRtRadiance)
    {
        m_rtRadiance->Destroy();
        m_rtRadiance.Reset();
    }

    if (m_hasDdgiProbes)
    {
        m_ddgi.Destroy();
    }

    SafeRelease(std::move(m_topLevelAccelerationStructures));
}

void RenderEnvironment::Initialize()
{
    m_particleSystem = CreateObject<ParticleSystem>();
    InitObject(m_particleSystem);

    m_gaussianSplatting = CreateObject<GaussianSplatting>();
    InitObject(m_gaussianSplatting);

    /// FIXME: GetCurrentView() should not be used here
    // m_rtRadiance = MakeUnique<RaytracingReflections>(
    //     RaytracingReflectionsConfig::FromConfig(),
    //     g_engine->GetCurrentView()->GetGBuffer());

    if (g_renderBackend->GetRenderConfig().IsRaytracingSupported()
        && g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        CreateTopLevelAccelerationStructures();

        m_rtRadiance->SetTopLevelAccelerationStructures(m_topLevelAccelerationStructures);
        m_rtRadiance->Create();

        m_ddgi.SetTopLevelAccelerationStructures(m_topLevelAccelerationStructures);
        m_ddgi.Init();

        m_hasRtRadiance = true;
        m_hasDdgiProbes = true;
    }
}

void RenderEnvironment::ApplyTLASUpdates(FrameBase* frame, RTUpdateStateFlags flags)
{
    Threads::AssertOnThread(g_renderThread);

    static const bool isRaytracingSupported = g_renderBackend->GetRenderConfig().IsRaytracingSupported();
    Assert(isRaytracingSupported);

    if (m_hasRtRadiance)
    {
        m_rtRadiance->ApplyTLASUpdates(flags);
    }

    if (m_hasDdgiProbes)
    {
        m_ddgi.ApplyTLASUpdates(flags);
    }
}

void RenderEnvironment::RenderRTRadiance(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    if (m_hasRtRadiance)
    {
        m_rtRadiance->Render(frame, renderSetup);
    }
}

void RenderEnvironment::RenderDDGIProbes(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(g_renderBackend->GetRenderConfig().IsRaytracingSupported());

    if (m_hasDdgiProbes)
    {
        m_ddgi.Render(frame, renderSetup);
    }
}

void RenderEnvironment::InitializeRT()
{
    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        if (m_hasRtRadiance)
        {
            m_rtRadiance->Destroy();
        }

        if (m_hasDdgiProbes)
        {
            m_ddgi.Destroy();
        }

        m_ddgi.SetTopLevelAccelerationStructures(m_topLevelAccelerationStructures);
        m_rtRadiance->SetTopLevelAccelerationStructures(m_topLevelAccelerationStructures);

        m_rtRadiance->Create();
        m_ddgi.Init();

        m_hasRtRadiance = true;
        m_hasDdgiProbes = true;
    }
    else
    {
        if (m_hasRtRadiance)
        {
            m_rtRadiance->Destroy();
        }

        if (m_hasDdgiProbes)
        {
            m_ddgi.Destroy();
        }

        m_hasRtRadiance = false;
        m_hasDdgiProbes = false;
    }

    m_rtInitialized = true;
}

bool RenderEnvironment::CreateTopLevelAccelerationStructures()
{
    HYP_SCOPE;

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return false;
    }

    // @FIXME: Hack solution since TLAS can only be created if it has a non-zero number of BLASes.
    // This whole thing should be reworked
    Handle<Mesh> defaultMesh = MeshBuilder::Cube();
    InitObject(defaultMesh);

    Handle<Material> defaultMaterial = CreateObject<Material>();
    InitObject(defaultMaterial);

    BLASRef blas;

    {
        TResourceHandle<RenderMesh> meshResourceHandle(defaultMesh->GetRenderResource());

        blas = meshResourceHandle->BuildBLAS(defaultMaterial);
    }

    DeferCreate(blas);

    for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
    {
        m_topLevelAccelerationStructures[frameIndex] = g_renderBackend->MakeTLAS();
        m_topLevelAccelerationStructures[frameIndex]->AddBLAS(blas);

        DeferCreate(m_topLevelAccelerationStructures[frameIndex]);
    }

    return true;
}

} // namespace hyperion
