/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/World.hpp>
#include <rendering/Camera.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/Handle.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCollectorContainer

void RenderCollectorContainer::AddScene(const Scene *scene)
{
    AssertThrow(scene != nullptr);
    AssertThrowMsg(scene->GetCamera().IsValid(), "Cannot acquire RenderCollector for Scene with no Camera attached.");

    const uint32 scene_index = scene->GetID().ToIndex();
    AssertThrow(scene_index < max_scenes);

    RenderCollector &render_collector = m_render_collectors_by_id_index[scene_index];
    render_collector.SetCamera(scene->GetCamera());

    if (scene->IsNonWorldScene()) {
        render_collector.SetRenderEnvironment(WeakHandle<RenderEnvironment> { });
    } else {
        render_collector.SetRenderEnvironment(scene->GetEnvironment());
    }

    const uint32 render_collector_index = m_num_render_collectors.Increment(1u, MemoryOrder::ACQUIRE_RELEASE);
    m_render_collectors[render_collector_index] = &render_collector;
}

void RenderCollectorContainer::RemoveScene(ID<Scene> id)
{
    AssertThrow(id.IsValid());
    
    m_num_render_collectors.Decrement(1u, MemoryOrder::RELEASE);

    RenderCollector &render_collector = m_render_collectors_by_id_index[id.ToIndex()];
    render_collector.SetCamera(Handle<Camera>::empty);
    render_collector.SetRenderEnvironment(WeakHandle<RenderEnvironment> { });
    render_collector.Reset();
}

#pragma endregion RenderCollectorContainer

#pragma region WorldRenderResources

WorldRenderResources::WorldRenderResources(World *world)
    : m_world(world)
{
}

WorldRenderResources::~WorldRenderResources() = default;

void WorldRenderResources::Initialize()
{
    HYP_SCOPE;
}

void WorldRenderResources::Destroy()
{
    HYP_SCOPE;
}

void WorldRenderResources::Update()
{
    HYP_SCOPE;
}

GPUBufferHolderBase *WorldRenderResources::GetGPUBufferHolder() const
{
    return nullptr;
}

void WorldRenderResources::PreRender(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint32 num_render_collectors = m_render_collector_container.NumRenderCollectors();

    for (uint32 i = 0; i < num_render_collectors; i++) {
        RenderCollector *render_collector = m_render_collector_container.GetRenderCollectorAtIndex(i);

        if (const Handle<Camera> &camera = render_collector->GetCamera()) {
            m_bound_cameras.PushBack(TResourceHandle(camera->GetRenderResources()));
        }
    }
}

void WorldRenderResources::PostRender(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    m_bound_cameras.Clear();
}

void WorldRenderResources::Render(renderer::Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint32 num_render_collectors = m_render_collector_container.NumRenderCollectors();

    for (uint32 i = 0; i < num_render_collectors; i++) {
        RenderCollector *render_collector = m_render_collector_container.GetRenderCollectorAtIndex(i);

        if (const WeakHandle<RenderEnvironment> &render_environment = render_collector->GetRenderEnvironment()) {
            render_environment.GetUnsafe()->RenderSubsystems(frame);
        }
    }
}

#pragma endregion WorldRenderResources

} // namespace hyperion