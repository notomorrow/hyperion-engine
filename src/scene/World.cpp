/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/World.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorSubsystem.hpp>
#endif

#include <scene/ecs/EntityManager.hpp>

#include <scene/world_grid/WorldGridSubsystem.hpp>

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

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
#define HYP_WORLD_ASYNC_SCENE_UPDATES

World::World()
    : HypObject(),
      m_detached_scenes(this),
      m_render_resource(nullptr)
{
    // AddSubsystem<WorldGridSubsystem>();
}

World::~World()
{
    if (IsInitCalled()) {
        for (const Handle<Scene> &scene : m_scenes) {
            if (!scene.IsValid()) {
                continue;
            }

            for (auto &it : m_subsystems) {
                it.second->OnSceneDetached(scene);
            }

            scene->SetWorld(nullptr);
        }

        m_scenes.Clear();

        for (auto &it : m_subsystems) {
            it.second->Shutdown();
        }
    }

    m_physics_world.Teardown();

    if (m_render_resource != nullptr) {
        m_render_resource->Unclaim();

        FreeResource(m_render_resource);

        m_render_resource = nullptr;
    }
}
    
void World::Init()
{
    if (IsInitCalled()) {
        return;
    }

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
    {
        if (m_render_resource != nullptr) {
            m_render_resource->Unclaim();

            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    // Update WorldRenderResource's RenderStats - done on the game thread so both the game thread and render thread can access it.
    // It is copied, so it will be delayed by 1-2 frames for the render thread to see the updated stats.
    AddDelegateHandler(g_engine->OnRenderStatsUpdated.Bind([this_weak = WeakHandleFromThis()](EngineRenderStats render_stats)
    {
        Threads::AssertOnThread(g_game_thread);
        
        Handle<World> world = this_weak.Lock();

        if (!world.IsValid()) {
            return;
        }

        if (!world->IsReady()) {
            return;
        }

        world->GetRenderResource().SetRenderStats(render_stats);
    }, g_game_thread));

    m_render_resource = AllocateResource<WorldRenderResource>(this);

    for (auto &it : m_subsystems) {
        it.second->Initialize();
    }

    for (const Handle<Scene> &scene : m_scenes) {
        scene->SetWorld(this);

        InitObject(scene);

        for (auto &it : m_subsystems) {
            it.second->OnSceneAttached(scene);
        }
    }

    // Add the now initialized scenes to the render resources
    // Note: call before Claim() so execution can happen inline and doesn't defer to the render thread.
    m_render_resource->Execute([this]()
    {
        for (const Handle<Scene> &scene : m_scenes) {
            m_render_resource->GetRenderCollectorContainer().AddScene(scene.GetID());
        }
    }, /* force_render_thread */ false);

    m_render_resource->Claim();
    
    HypObject::Init();

    m_physics_world.Init();

    SetReady(true);
}

void World::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    AssertReady();

    m_game_state.delta_time = delta;

    m_physics_world.Tick(delta);

#ifdef HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
    Array<Task<void>> update_subsystem_tasks;
    update_subsystem_tasks.Reserve(m_subsystems.Size());

    for (auto &it : m_subsystems) {
        Subsystem *subsystem = it.second.Get();

        if (subsystem->RequiresUpdateOnGameThread()) {
            continue;
        }

        subsystem->PreUpdate(delta);

        update_subsystem_tasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
        {
            HYP_NAMED_SCOPE_FMT("Update subsystem: {}", subsystem->InstanceClass()->GetName());

            subsystem->Update(delta);
        }));
    }

    for (auto &it : m_subsystems) {
        Subsystem *subsystem = it.second.Get();

        if (!subsystem->RequiresUpdateOnGameThread()) {
            continue;
        }

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }

    for (Task<void> &task : update_subsystem_tasks) {
        task.Await();
    }

    update_subsystem_tasks.Clear();
#else
    for (auto &it : m_subsystems) {
        Subsystem *subsystem = it.second.Get();

        subsystem->Update(delta);
    }
#endif

    m_render_resource->GetEnvironment()->Update(delta);

    Array<EntityManager *> entity_managers;
    entity_managers.Reserve(m_scenes.Size());

    for (uint32 index = 0; index < m_scenes.Size(); index++) {
        const Handle<Scene> &scene = m_scenes[index];

        if (!scene.IsValid()) {
            continue;
        }

        // sanity checks
        AssertThrow(scene->GetWorld() == this);
        AssertThrow(!(scene->GetFlags() & SceneFlags::DETACHED));

        scene->Update(delta);

        entity_managers.PushBack(scene->GetEntityManager().Get());
    }

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
    Array<Task<void>> collect_entities_tasks;
    collect_entities_tasks.Reserve(m_scenes.Size());
#endif

    // Ensure Scene::Update() and EntityManager::FlushCommandQueue() or called on all scenes before we kickoff async updates for their entity managers.
    for (EntityManager *entity_manager : entity_managers) {
        HYP_NAMED_SCOPE("Call BeginAsyncUpdate on EntityManager");

        entity_manager->BeginAsyncUpdate(delta);
    }

    for (EntityManager *entity_manager : entity_managers) {
        HYP_NAMED_SCOPE("Call EndAsyncUpdate on EntityManager");

        entity_manager->EndAsyncUpdate();
    }

    for (uint32 index = 0; index < m_scenes.Size(); index++) {
        HYP_NAMED_SCOPE("Scene entity collection");

        const Handle<Scene> &scene = m_scenes[index];
        AssertThrow(scene.IsValid());

        // sanity check
        AssertThrow(scene->GetWorld() == this);

        scene->EnqueueRenderUpdates();

        if (!scene->IsForegroundScene() || (scene->GetFlags() & SceneFlags::UI)) {
            continue;
        }

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
        collect_entities_tasks.PushBack(TaskSystem::GetInstance().Enqueue([this, index]
        {
            const Handle<Scene> &scene = m_scenes[index];
            AssertThrow(scene.IsValid());

            // sanity check
            AssertThrow(scene->GetWorld() == this);

            RenderCollector &render_collector = m_render_resource->GetRenderCollectorForScene(scene->GetID());

            scene->CollectEntities(render_collector, scene->GetCamera());
        }));
#else
        // sanity check
        AssertThrow(scene->GetWorld() == this);

        RenderCollector &render_collector = m_render_resource->GetRenderCollectorForScene(scene->GetID());

        scene->CollectEntities(render_collector, scene->GetCamera());
#endif
    }

#ifdef HYP_WORLD_ASYNC_SCENE_UPDATES
    for (Task<void> &task : collect_entities_tasks) {
        task.Await();
    }
#endif

    m_game_state.game_time += delta;
}

RC<Subsystem> World::AddSubsystem(TypeID type_id, const RC<Subsystem> &subsystem)
{
    HYP_SCOPE;

    if (!subsystem) {
        return nullptr;
    }

    // If World is already initialized, ensure that the call is made on the correct thread
    // otherwise, it is safe to call this function from the World constructor (main thread)
    if (IsInitCalled()) {
        Threads::AssertOnThread(g_game_thread);
    }

    subsystem->SetWorld(this);

    const auto it = m_subsystems.Find(type_id);
    AssertThrowMsg(it == m_subsystems.End(), "Subsystem already exists in World");

    auto insert_result = m_subsystems.Set(type_id, subsystem);

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsInitCalled()) {
        if (insert_result.second) {
            insert_result.first->second->Initialize();

            for (Handle<Scene> &scene : m_scenes) {
                insert_result.first->second->OnSceneAttached(scene);
            }
        }
    }

    return insert_result.first->second;
}

Subsystem *World::GetSubsystem(TypeID type_id) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.Find(type_id);

    if (it == m_subsystems.End()) {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem *World::GetSubsystemByName(WeakName name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.FindIf([name](const Pair<TypeID, RC<Subsystem>> &item)
    {
        if (!item.second) {
            return false;
        }

        const HypClass *hyp_class = HypClassRegistry::GetInstance().GetClass(item.second.GetTypeID());

        if (!hyp_class) {
            return false;
        }

        return hyp_class->GetName() == name;
    });

    if (it == m_subsystems.End()) {
        return nullptr;
    }

    return it->second.Get();
}

void World::StartSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (m_game_state.mode == GameStateMode::SIMULATING) {
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
    
    if (m_game_state.mode != GameStateMode::SIMULATING) {
        return;
    }

    OnGameStateChange(this, GameStateMode::EDITOR);

    m_game_state.game_time = 0.0f;
    m_game_state.delta_time = 0.0f;
    m_game_state.mode = GameStateMode::EDITOR;
}

void World::AddScene(const Handle<Scene> &scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    if (!scene.IsValid()) {
        return;
    }

    if (m_scenes.Contains(scene)) {
        return;
    }

    scene->SetWorld(this);

    if (IsInitCalled()) {
        InitObject(scene);

        for (auto &it : m_subsystems) {
            it.second->OnSceneAttached(scene);
        }

        m_render_resource->AddScene(scene);
    }

    m_scenes.PushBack(scene);
}

bool World::RemoveScene(const WeakHandle<Scene> &scene_weak)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread);

    typename Array<Handle<Scene>>::Iterator it = m_scenes.FindAs(scene_weak);
    
    if (it == m_scenes.End()) {
        return false;
    }

    Handle<Scene> scene = *it;
    m_scenes.Erase(it);

    if (scene.IsValid()) {
        scene->SetWorld(nullptr);

        if (IsInitCalled()) {
            for (auto &it : m_subsystems) {
                it.second->OnSceneDetached(scene);
            }

            Task<bool> task = m_render_resource->RemoveScene(scene->GetID());
            return task.Await();
        }
    }

    return true;
}

bool World::HasScene(ID<Scene> scene_id) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    return m_scenes.FindIf([scene_id](const Handle<Scene> &scene)
    {
        return scene.GetID() == scene_id;
    }) != m_scenes.End();
}

const Handle<Scene> &World::GetSceneByName(Name name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    const auto it = m_scenes.FindIf([name](const Handle<Scene> &scene)
    {
        return scene->GetName() == name;
    });

    return it != m_scenes.End() ? *it : Handle<Scene>::empty;
}

const Handle<Scene> &World::GetDetachedScene(const ThreadID &thread_id)
{
    return m_detached_scenes.GetDetachedScene(thread_id);
}

EngineRenderStats *World::GetRenderStats() const
{
    if (!IsReady()) {
        return nullptr;
    }

    return &const_cast<EngineRenderStats &>(m_render_resource->GetRenderStats());
}

} // namespace hyperion
