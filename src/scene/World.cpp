/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>
#include <scene/View.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorSubsystem.hpp>
#endif

#include <scene/ecs/EntityManager.hpp>

#include <scene/world_grid/WorldGrid.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderView.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_VIEW_UPDATES

World::World()
    : HypObject(),
      m_world_grid(CreateObject<WorldGrid>(this)),
      m_detached_scenes(this),
      m_render_resource(nullptr)
{
}

World::~World()
{
    if (IsInitCalled())
    {
        for (const Handle<Scene>& scene : m_scenes)
        {
            if (!scene.IsValid())
            {
                continue;
            }

            OnSceneRemoved(this, scene);

            for (auto& it : m_subsystems)
            {
                it.second->OnSceneDetached(scene);
            }

            if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                    {
                        continue;
                    }

                    view->RemoveScene(scene);
                }
            }

            scene->SetWorld(nullptr);
        }

        m_scenes.Clear();

        for (const Handle<View>& view : m_views)
        {
            if (!view.IsValid())
            {
                continue;
            }

            m_render_resource->RemoveView(&view->GetRenderResource());
        }

        m_views.Clear();

        for (auto& it : m_subsystems)
        {
            it.second->Shutdown();
        }
    }

    m_physics_world.Teardown();

    if (m_render_resource != nullptr)
    {
        m_render_resource->DecRef();

        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }

    if (m_world_grid.IsValid())
    {
        m_world_grid->Shutdown();
    }
}

void World::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_render_resource != nullptr)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!view.IsValid())
                    {
                        continue;
                    }

                    m_render_resource->RemoveView(&view->GetRenderResource());
                }

                m_views.Clear();

                m_render_resource->DecRef();

                FreeResource(m_render_resource);

                m_render_resource = nullptr;
            }

            if (m_world_grid.IsValid())
            {
                m_world_grid->Shutdown();
            }
        }));

    // Update RenderWorld's RenderStats - done on the game thread so both the game thread and render thread can access it.
    // It is copied, so it will be delayed by 1-2 frames for the render thread to see the updated stats.
    AddDelegateHandler(g_engine->OnRenderStatsUpdated.Bind([this_weak = WeakHandleFromThis()](EngineRenderStats render_stats)
        {
            Threads::AssertOnThread(g_game_thread);

            Handle<World> world = this_weak.Lock();

            if (!world.IsValid())
            {
                return;
            }

            if (!world->IsReady())
            {
                return;
            }

            world->GetRenderResource().SetRenderStats(render_stats);
        },
        g_game_thread));

    m_render_resource = AllocateResource<RenderWorld>(this);

    WorldShaderData shader_data {};
    shader_data.game_time = m_game_state.game_time;
    m_render_resource->SetBufferData(shader_data);

    for (auto& it : m_subsystems)
    {
        it.second->Initialize();
    }

    InitObject(m_world_grid);

    for (const Handle<View>& view : m_views)
    {
        InitObject(view);

        m_render_resource->AddView(TResourceHandle<RenderView>(view->GetRenderResource()));
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->SetWorld(this);

        InitObject(scene);

        OnSceneAdded(this, scene);

        for (auto& it : m_subsystems)
        {
            it.second->OnSceneAttached(scene);
        }

        if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        m_render_resource->AddScene(TResourceHandle<RenderScene>(scene->GetRenderResource()));
    }

    m_render_resource->IncRef();

    m_physics_world.Init();

    SetReady(true);
}

void World::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    m_game_state.delta_time = delta;

    WorldShaderData shader_data {};
    shader_data.game_time = m_game_state.game_time;
    m_render_resource->SetBufferData(shader_data);

    m_world_grid->Update(delta);

    m_physics_world.Tick(delta);

#ifdef HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
    Array<Task<void>> update_subsystem_tasks;
    update_subsystem_tasks.Reserve(m_subsystems.Size());

    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        if (subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        update_subsystem_tasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
            {
                HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->InstanceClass()->GetName());

                subsystem->Update(delta);
            }));
    }

    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        if (!subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }

    for (Task<void>& task : update_subsystem_tasks)
    {
        task.Await();
    }

    update_subsystem_tasks.Clear();
#else
    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        subsystem->Update(delta);
    }
#endif

    m_render_resource->GetEnvironment()->Update(delta);

    Array<EntityManager*> entity_managers;
    entity_managers.Reserve(m_scenes.Size());

    for (uint32 index = 0; index < m_scenes.Size(); index++)
    {
        const Handle<Scene>& scene = m_scenes[index];

        if (!scene.IsValid())
        {
            continue;
        }

        // sanity checks
        AssertThrow(scene->GetWorld() == this);
        AssertThrow(!(scene->GetFlags() & SceneFlags::DETACHED));

        scene->Update(delta);

        entity_managers.PushBack(scene->GetEntityManager().Get());
    }

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
    Array<Task<void>> update_views_tasks;
    update_views_tasks.Reserve(m_views.Size());
#endif

    // Ensure Scene::Update() and EntityManager::FlushCommandQueue() or called on all scenes before we kickoff async updates for their entity managers.
    for (EntityManager* entity_manager : entity_managers)
    {
        HYP_NAMED_SCOPE("Call BeginAsyncUpdate on EntityManager");

        entity_manager->BeginAsyncUpdate(delta);
    }

    for (EntityManager* entity_manager : entity_managers)
    {
        HYP_NAMED_SCOPE("Call EndAsyncUpdate on EntityManager");

        entity_manager->EndAsyncUpdate();
    }

    for (uint32 index = 0; index < m_views.Size(); index++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        const Handle<View>& view = m_views[index];
        AssertThrow(view.IsValid());

        // if (!view->GetScene()->IsForegroundScene() || (view->GetScene()->GetFlags() & SceneFlags::UI))
        // {
        //     continue;
        // }

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
        update_views_tasks.PushBack(TaskSystem::GetInstance().Enqueue([view = m_views[index].Get(), delta]
            {
                view->Update(delta);
            }));
#else
        view->Update();
#endif
    }

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
    for (Task<void>& task : update_views_tasks)
    {
        task.Await();
    }
#endif

    m_game_state.game_time += delta;
}

RC<Subsystem> World::AddSubsystem(TypeID type_id, const RC<Subsystem>& subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return nullptr;
    }

    // temp; removed for debugging
    // Threads::AssertOnThread(g_game_thread);

    subsystem->SetWorld(this);

    const auto it = m_subsystems.Find(type_id);
    AssertThrowMsg(it == m_subsystems.End(), "Subsystem already exists in World");

    auto insert_result = m_subsystems.Set(type_id, subsystem);

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsInitCalled())
    {
        if (insert_result.second)
        {
            insert_result.first->second->Initialize();

            for (Handle<Scene>& scene : m_scenes)
            {
                insert_result.first->second->OnSceneAttached(scene);
            }
        }
    }

    return insert_result.first->second;
}

Subsystem* World::GetSubsystem(TypeID type_id) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.Find(type_id);

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem* World::GetSubsystemByName(WeakName name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.FindIf([name](const Pair<TypeID, RC<Subsystem>>& item)
        {
            if (!item.second)
            {
                return false;
            }

            const HypClass* hyp_class = HypClassRegistry::GetInstance().GetClass(item.second.GetTypeID());

            if (!hyp_class)
            {
                return false;
            }

            return hyp_class->GetName() == name;
        });

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

void World::StartSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (m_game_state.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    OnGameStateChange(this, GameStateMode::SIMULATING);

    m_game_state.game_time = 0.0f;
    m_game_state.delta_time = 0.0f;
    m_game_state.mode = GameStateMode::SIMULATING;
}

void World::StopSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (m_game_state.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    OnGameStateChange(this, GameStateMode::EDITOR);

    m_game_state.game_time = 0.0f;
    m_game_state.delta_time = 0.0f;
    m_game_state.mode = GameStateMode::EDITOR;
}

void World::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        HYP_LOG(Scene, Warning, "Scene {} already exists in world", scene->GetName());

        return;
    }

    scene->SetWorld(this);

    if (IsInitCalled())
    {
        InitObject(scene);

        OnSceneAdded(this, scene);

        for (auto& it : m_subsystems)
        {
            it.second->OnSceneAttached(scene);
        }

        if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
        {
            for (const Handle<View>& view : m_views)
            {
                if (!(view->GetFlags() & ViewFlags::ALL_WORLD_SCENES))
                {
                    continue;
                }

                view->AddScene(scene);
            }
        }

        m_render_resource->AddScene(TResourceHandle<RenderScene>(scene->GetRenderResource()));
    }

    m_scenes.PushBack(scene);
}

bool World::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);
    // @TODO RemoveScene needs to trigger a scene detach event for all rendersubsystems

    typename Array<Handle<Scene>>::Iterator it = m_scenes.Find(scene);

    if (it == m_scenes.End())
    {
        return false;
    }

    Handle<Scene> scene_copy = *it;

    m_scenes.Erase(it);

    if (scene.IsValid())
    {
        OnSceneRemoved(this, scene);

        scene->SetWorld(nullptr);

        if (IsInitCalled())
        {
            m_render_resource->RemoveScene(&scene->GetRenderResource());

            for (auto& it : m_subsystems)
            {
                it.second->OnSceneDetached(scene);
            }

            for (const Handle<View>& view : m_views)
            {
                view->RemoveScene(scene);
            }
        }
    }

    return true;
}

bool World::HasScene(ID<Scene> scene_id) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    return m_scenes.FindIf([scene_id](const Handle<Scene>& scene)
               {
                   return scene.GetID() == scene_id;
               })
        != m_scenes.End();
}

const Handle<Scene>& World::GetSceneByName(Name name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    const auto it = m_scenes.FindIf([name](const Handle<Scene>& scene)
        {
            return scene->GetName() == name;
        });

    return it != m_scenes.End() ? *it : Handle<Scene>::empty;
}

const Handle<Scene>& World::GetDetachedScene(const ThreadID& thread_id)
{
    return m_detached_scenes.GetDetachedScene(thread_id);
}

void World::AddView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!view.IsValid())
    {
        return;
    }

    m_views.PushBack(view);

    if (IsInitCalled())
    {
        // Add all scenes to the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene.IsValid())
                {
                    continue;
                }

                if ((scene->GetFlags() & (SceneFlags::FOREGROUND | SceneFlags::UI | SceneFlags::DETACHED)) == SceneFlags::FOREGROUND)
                {
                    view->AddScene(scene);
                }
            }
        }

        InitObject(view);

        m_render_resource->AddView(TResourceHandle<RenderView>(view->GetRenderResource()));
    }
}

void World::RemoveView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!view.IsValid())
    {
        return;
    }

    typename Array<Handle<View>>::Iterator it = m_views.Find(view);

    if (IsInitCalled())
    {
        // Remove all scenes from the view, if the view should collect all world scenes
        if (view->GetFlags() & ViewFlags::ALL_WORLD_SCENES)
        {
            for (const Handle<Scene>& scene : m_scenes)
            {
                if (!scene.IsValid())
                {
                    continue;
                }

                view->RemoveScene(scene);
            }
        }

        if (view->IsReady())
        {
            m_render_resource->RemoveView(&view->GetRenderResource());
        }
    }

    if (it != m_views.End())
    {
        m_views.Erase(it);
    }
}

EngineRenderStats* World::GetRenderStats() const
{
    if (!IsReady())
    {
        return nullptr;
    }

    return &const_cast<EngineRenderStats&>(m_render_resource->GetRenderStats());
}

} // namespace hyperion
