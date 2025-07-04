/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/Light.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

PointLightShadowRenderer::PointLightShadowRenderer(Name name, const Handle<Scene>& parentScene, const Handle<Light>& light, const Vec2u& extent)
    : RenderSubsystem(name),
      m_parentScene(parentScene),
      m_light(light),
      m_extent(extent)
{
}

PointLightShadowRenderer::~PointLightShadowRenderer()
{
    HYP_SYNC_RENDER(); // wait for render commands to finish
}

void PointLightShadowRenderer::Init()
{
    Assert(m_parentScene.IsValid());
    Assert(m_parentScene->IsReady());

    RenderShadowMap* shadowMap = g_renderGlobalState->ShadowMapAllocator->AllocateShadowMap(
        SMT_OMNI,
        SMF_VSM,
        m_extent);
    Assert(shadowMap != nullptr, "Failed to allocate shadow map");

    m_shadowMap = TResourceHandle<RenderShadowMap>(*shadowMap);
    m_aabb = m_light->GetAABB();

    m_envProbe = m_parentScene->GetEntityManager()->AddEntity<EnvProbe>(EPT_SHADOW, m_aabb, m_extent);
    InitObject(m_envProbe);

    m_envProbe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadowMap));

    if (m_light.IsValid())
    {
        InitObject(m_light);

        m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadowMap));
    }
}

void PointLightShadowRenderer::OnRemoved()
{
    if (m_light.IsValid())
    {
        m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());
    }

    // if (m_envProbe.IsValid())
    // {
    //     m_envProbe->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());

    //     m_envProbe->GetRenderResource().DecRef(); // temp testing
    // }

    // m_envProbe.Reset();

    if (m_shadowMap)
    {
        RenderShadowMap* shadowMap = m_shadowMap.Get();

        m_shadowMap.Reset();

        if (!g_renderGlobalState->ShadowMapAllocator->FreeShadowMap(shadowMap))
        {
            HYP_FAIL("Failed to free shadow map!");
        }
    }
}

void PointLightShadowRenderer::OnUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    Assert(m_envProbe.IsValid());
    Assert(m_light.IsValid());

    const BoundingBox& envProbeAabb = m_envProbe->GetAABB();
    const BoundingBox lightAabb = m_light->GetAABB();

    if (envProbeAabb != lightAabb)
    {
        m_envProbe->SetAABB(lightAabb);
    }
}

} // namespace hyperion
