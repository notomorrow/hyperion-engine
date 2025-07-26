/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderWorld.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/EnvProbe.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region RenderWorld

RenderWorld::RenderWorld(World* world)
    : m_world(world),
      m_renderEnvironment(MakeUnique<RenderEnvironment>()),
      m_bufferData {}
{
}

RenderWorld::~RenderWorld() = default;

void RenderWorld::AddView(const Handle<View>& view)
{
    HYP_SCOPE;

    if (!view.IsValid())
    {
        return;
    }

    Execute([this, view = view]()
        {
            m_views.PushBack(view);

            std::sort(m_views.Begin(), m_views.End(), [](const Handle<View>& a, const Handle<View>& b)
                {
                    return uint32(b->GetPriority()) < uint32(a->GetPriority());
                });
        });
}

void RenderWorld::RemoveView(View* view)
{
    HYP_SCOPE;

    if (!view)
    {
        return;
    }

    Execute([this, view]()
        {
            auto it = m_views.FindIf([view](auto&& item)
                {
                    return item.Get() == view;
                });

            if (it != m_views.End())
            {
                m_views.Erase(it);
            }
        });
}

// void RenderWorld::RenderAddShadowMap(const TResourceHandle<RenderShadowMap> &shadowMapResourceHandle)
// {
//     HYP_SCOPE;

//     if (!shadowMapResourceHandle) {
//         return;
//     }

//     Execute([this, shadowMapResourceHandle]()
//     {
//         m_shadowMapResourceHandles.PushBack(std::move(shadowMapResourceHandle));
//     }, /* forceOwnerThread */ true);
// }

// void RenderWorld::RenderRemoveShadowMap(const RenderShadowMap *shadowMap)
// {
//     HYP_SCOPE;

//     if (!shadowMap) {
//         return;
//     }

//     Execute([this, shadowMap]()
//     {
//         auto it = m_shadowMapResourceHandles.FindIf([shadowMap](const TResourceHandle<RenderShadowMap> &item)
//         {
//             return item.Get() == shadowMap;
//         });

//         if (it != m_shadowMapResourceHandles.End()) {
//             m_shadowMapResourceHandles.Erase(it);
//         }
//     }, /* forceOwnerThread */ true);
// }

const RenderStats& RenderWorld::GetRenderStats() const
{
    HYP_SCOPE;
    if (Threads::IsOnThread(g_renderThread))
    {
        return m_renderStats[ThreadType::THREAD_TYPE_RENDER];
    }
    else if (Threads::IsOnThread(g_gameThread))
    {
        return m_renderStats[ThreadType::THREAD_TYPE_GAME];
    }
    else
    {
        HYP_UNREACHABLE();
    }
}

void RenderWorld::SetRenderStats(const RenderStats& renderStats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    m_renderStats[ThreadType::THREAD_TYPE_GAME] = renderStats;

    Execute([this, renderStats]()
        {
            m_renderStats[ThreadType::THREAD_TYPE_RENDER] = renderStats;
        });
}

void RenderWorld::Initialize_Internal()
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Info, "Initializing RenderWorld for World with Id: {}", m_world->Id());

    m_renderEnvironment->Initialize();

    UpdateBufferData();
}

void RenderWorld::Destroy_Internal()
{
    HYP_SCOPE;

    m_views.Clear();
}

void RenderWorld::Update_Internal()
{
    HYP_SCOPE;
}

void RenderWorld::SetBufferData(const WorldShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            // Preserve the frame counter
            const uint32 frameCounter = m_bufferData.frameCounter;

            m_bufferData = bufferData;
            m_bufferData.frameCounter = frameCounter;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderWorld::UpdateBufferData()
{
    HYP_SCOPE;

    *static_cast<WorldShaderData*>(m_bufferAddress) = m_bufferData;

    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
}

GpuBufferHolderBase* RenderWorld::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_WORLDS];
}

void RenderWorld::Render(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
}

void RenderWorld::PostRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
}

#pragma endregion RenderWorld

} // namespace hyperion
