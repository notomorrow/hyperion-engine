/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/World.hpp>

#include <scene/camera/PerspectiveCamera.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#if 0
ReflectionProbeRenderer::ReflectionProbeRenderer(Name name, const Handle<EnvProbe>& envProbe)
    : RenderSubsystem(name),
      m_envProbe(envProbe)
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Init()
{
    InitObject(m_envProbe);
}

void ReflectionProbeRenderer::OnRemoved()
{
    HYP_SYNC_RENDER(); // to prevent dangling pointers
}

void ReflectionProbeRenderer::OnUpdate(float delta)
{
    struct RENDER_COMMAND(RenderReflectionProbe)
        : RenderCommand
    {
        RenderWorld* world;
        RenderEnvProbe* envProbe;

        RENDER_COMMAND(RenderReflectionProbe)(RenderWorld* world, RenderEnvProbe* envProbe)
            : world(world),
              envProbe(envProbe)
        {
            // world->IncRef();
            // envProbe->IncRef();
        }

        virtual ~RENDER_COMMAND(RenderReflectionProbe)() override
        {
            world->DecRef();
            envProbe->DecRef();
        }

        virtual RendererResult operator()() override
        {
            FrameBase* frame = g_renderBackend->GetCurrentFrame();

            RenderSetup renderSetup { world, nullptr };

            envProbe->Render(frame, renderSetup);

            HYPERION_RETURN_OK;
        }
    };

    if (!m_envProbe.IsValid())
    {
        return;
    }

    if (!m_envProbe->NeedsRender())
    {
        return;
    }

    g_engine->GetWorld()->GetRenderResource().IncRef();
    m_envProbe->GetRenderResource().IncRef();
    PUSH_RENDER_COMMAND(RenderReflectionProbe, &g_engine->GetWorld()->GetRenderResource(), &m_envProbe->GetRenderResource());

    m_envProbe->SetNeedsRender(false);
}
#endif

} // namespace hyperion
