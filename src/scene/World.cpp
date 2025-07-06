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
#include <rendering/RenderView.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#define HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
// #define HYP_WORLD_ASYNC_VIEW_UPDATES

World::World()
    : HypObject(),
      m_worldGrid(CreateObject<WorldGrid>(this)),
      m_detachedScenes(this),
      m_renderResource(nullptr)
{
}

World::~World()
{
    if (IsReady())
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

            m_renderResource->RemoveView(&view->GetRenderResource());
        }

        m_views.Clear();

        for (auto& it : m_subsystems)
        {
            const Handle<Subsystem>& subsystem = it.second;
            Assert(subsystem.IsValid());

            it.second->OnRemovedFromWorld();
        }
    }

    m_physicsWorld.Teardown();

    if (m_renderResource != nullptr)
    {
        m_renderResource->DecRef();

        FreeResource(m_renderResource);

        m_renderResource = nullptr;
    }

    if (m_worldGrid.IsValid())
    {
        m_worldGrid->Shutdown();
    }
}

void World::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]
        {
            if (m_renderResource != nullptr)
            {
                for (const Handle<View>& view : m_views)
                {
                    if (!view.IsValid())
                    {
                        continue;
                    }

                    m_renderResource->RemoveView(&view->GetRenderResource());
                }

                m_views.Clear();

                m_renderResource->DecRef();

                FreeResource(m_renderResource);

                m_renderResource = nullptr;
            }

            if (m_worldGrid.IsValid())
            {
                m_worldGrid->Shutdown();
            }
        }));

    // Update RenderWorld's RenderStats - done on the game thread so both the game thread and render thread can access it.
    // It is copied, so it will be delayed by 1-2 frames for the render thread to see the updated stats.
    AddDelegateHandler(g_engine->OnRenderStatsUpdated.Bind([thisWeak = WeakHandleFromThis()](RenderStats renderStats)
        {
            Threads::AssertOnThread(g_gameThread);

            Handle<World> world = thisWeak.Lock();

            if (!world.IsValid())
            {
                return;
            }

            if (!world->IsReady())
            {
                return;
            }

            world->GetRenderResource().SetRenderStats(renderStats);
        },
        g_gameThread));

    m_renderResource = AllocateResource<RenderWorld>(this);

    WorldShaderData shaderData {};
    shaderData.gameTime = m_gameState.gameTime;
    m_renderResource->SetBufferData(shaderData);

    for (auto& it : m_subsystems)
    {
        const Handle<Subsystem>& subsystem = it.second;
        Assert(subsystem.IsValid());

        InitObject(subsystem);

        subsystem->OnAddedToWorld();
    }

    InitObject(m_worldGrid);

    for (const Handle<View>& view : m_views)
    {
        InitObject(view);

        m_renderResource->AddView(TResourceHandle<RenderView>(view->GetRenderResource()));
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
    }

    m_renderResource->IncRef();

    m_physicsWorld.Init();

    SetReady(true);
}

void World::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    AssertReady();

    m_gameState.deltaTime = delta;

    WorldShaderData shaderData {};
    shaderData.gameTime = m_gameState.gameTime;
    m_renderResource->SetBufferData(shaderData);

    m_worldGrid->Update(delta);

    m_physicsWorld.Tick(delta);

#ifdef HYP_WORLD_ASYNC_SUBSYSTEM_UPDATES
    Array<Task<void>> updateSubsystemTasks;

    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        if (subsystem->RequiresUpdateOnGameThread())
        {
            continue;
        }

        subsystem->PreUpdate(delta);

        updateSubsystemTasks.PushBack(TaskSystem::GetInstance().Enqueue([subsystem, delta]
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

    for (Task<void>& task : updateSubsystemTasks)
    {
        task.Await();
    }

    updateSubsystemTasks.Clear();
#else
    for (auto& it : m_subsystems)
    {
        Subsystem* subsystem = it.second.Get();

        subsystem->PreUpdate(delta);
        subsystem->Update(delta);
    }
#endif

    Array<EntityManager*> entityManagers;
    entityManagers.Reserve(m_scenes.Size());

    // @TODO Collect all scenes, envgrids, envprobes for Views into a single list to call Update() on them all.
    for (uint32 index = 0; index < m_scenes.Size(); index++)
    {
        const Handle<Scene>& scene = m_scenes[index];

        if (!scene.IsValid())
        {
            continue;
        }

        // sanity checks
        Assert(scene->GetWorld() == this);
        Assert(!(scene->GetFlags() & SceneFlags::DETACHED));

        scene->Update(delta);

        entityManagers.PushBack(scene->GetEntityManager().Get());
    }

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
    Array<Task<void>> updateViewsTasks;
    updateViewsTasks.Reserve(m_views.Size());
#endif

    for (EntityManager* entityManager : entityManagers)
    {
        HYP_NAMED_SCOPE("Call BeginAsyncUpdate on EntityManager");

        entityManager->BeginAsyncUpdate(delta);
    }

    for (EntityManager* entityManager : entityManagers)
    {
        HYP_NAMED_SCOPE("Call EndAsyncUpdate on EntityManager");

        entityManager->EndAsyncUpdate();
    }

    for (uint32 index = 0; index < m_views.Size(); index++)
    {
        HYP_NAMED_SCOPE("Per-view entity collection");

        const Handle<View>& view = m_views[index];
        Assert(view.IsValid());

        // View must be updated on the game thread as it mutates the scene's octree state
        view->UpdateVisibility();

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
        updateViewsTasks.PushBack(TaskSystem::GetInstance().Enqueue([view = m_views[index].Get(), delta]
            {
                view->Update(delta);
            }));
#else
        view->Update(delta);
#endif
    }

#ifdef HYP_WORLD_ASYNC_VIEW_UPDATES
    for (Task<void>& task : updateViewsTasks)
    {
        task.Await();
    }
#endif

    m_gameState.gameTime += delta;
}

Handle<Subsystem> World::AddSubsystem(TypeId typeId, const Handle<Subsystem>& subsystem)
{
    HYP_SCOPE;

    if (!subsystem)
    {
        return nullptr;
    }

    Threads::AssertOnThread(g_gameThread);

    subsystem->SetWorld(this);

    const auto it = m_subsystems.Find(typeId);
    Assert(it == m_subsystems.End(), "Subsystem of type %s already exists in World", *subsystem->InstanceClass()->GetName());

    auto insertResult = m_subsystems.Set(typeId, subsystem);

    // Create a new Handle, calling OnAddedToWorld() may add new subsystems which would invalidate the iterator
    Handle<Subsystem> newSubsystem = insertResult.first->second;
    Assert(newSubsystem.IsValid());

    // If World is already initialized, initialize the subsystem
    // otherwise, it will be initialized when World::Init() is called
    if (IsReady())
    {
        if (insertResult.second)
        {
            InitObject(newSubsystem);

            newSubsystem->OnAddedToWorld();

            for (Handle<Scene>& scene : m_scenes)
            {
                newSubsystem->OnSceneAttached(scene);
            }
        }
    }

    return newSubsystem;
}

Subsystem* World::GetSubsystem(TypeId typeId) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

Subsystem* World::GetSubsystemByName(WeakName name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const auto it = m_subsystems.FindIf([name](const Pair<TypeId, Handle<Subsystem>>& item)
        {
            if (!item.second)
            {
                return false;
            }

            const HypClass* hypClass = item.second->InstanceClass();

            return hypClass->GetName() == name;
        });

    if (it == m_subsystems.End())
    {
        return nullptr;
    }

    return it->second.Get();
}

bool World::RemoveSubsystem(Subsystem* subsystem)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!subsystem)
    {
        return false;
    }

    const TypeId typeId = subsystem->InstanceClass()->GetTypeId();

    auto it = m_subsystems.Find(typeId);

    if (it == m_subsystems.End())
    {
        return false;
    }

    Assert(it->second.Get() == subsystem);

    if (IsReady())
    {
        for (const Handle<Scene>& scene : m_scenes)
        {
            if (!scene.IsValid())
            {
                continue;
            }

            subsystem->OnSceneDetached(scene);
        }

        subsystem->OnRemovedFromWorld();
    }

    subsystem->SetWorld(nullptr);

    m_subsystems.Erase(it);

    return true;
}

void World::StartSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (m_gameState.mode == GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::SIMULATING;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::SIMULATING);
}

void World::StopSimulating()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (m_gameState.mode != GameStateMode::SIMULATING)
    {
        return;
    }

    const GameStateMode previousGameStateMode = m_gameState.mode;

    m_gameState.gameTime = 0.0f;
    m_gameState.deltaTime = 0.0f;
    m_gameState.mode = GameStateMode::EDITOR;

    OnGameStateChange(this, previousGameStateMode, GameStateMode::EDITOR);
}

void World::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

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

    if (IsReady())
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
    }

    m_scenes.PushBack(scene);
}

bool World::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    // @TODO RemoveScene needs to trigger a scene detach event for all rendersubsystems

    typename Array<Handle<Scene>>::Iterator it = m_scenes.Find(scene);

    if (it == m_scenes.End())
    {
        return false;
    }

    Handle<Scene> sceneCopy = *it;

    m_scenes.Erase(it);

    if (scene.IsValid())
    {
        OnSceneRemoved(this, scene);

        scene->SetWorld(nullptr);

        if (IsReady())
        {
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

bool World::HasScene(ObjId<Scene> sceneId) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    return m_scenes.FindIf([sceneId](const Handle<Scene>& scene)
               {
                   return scene.Id() == sceneId;
               })
        != m_scenes.End();
}

const Handle<Scene>& World::GetSceneByName(Name name) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_gameThread);

    const auto it = m_scenes.FindIf([name](const Handle<Scene>& scene)
        {
            return scene->GetName() == name;
        });

    return it != m_scenes.End() ? *it : Handle<Scene>::empty;
}

const Handle<Scene>& World::GetDetachedScene(const ThreadId& threadId)
{
    return m_detachedScenes.GetDetachedScene(threadId);
}

void World::AddView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!view.IsValid())
    {
        return;
    }

    m_views.PushBack(view);

    if (IsReady())
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

        m_renderResource->AddView(TResourceHandle<RenderView>(view->GetRenderResource()));
    }
}

void World::RemoveView(const Handle<View>& view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    if (!view.IsValid())
    {
        return;
    }

    typename Array<Handle<View>>::Iterator it = m_views.Find(view);

    if (IsReady())
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
            m_renderResource->RemoveView(&view->GetRenderResource());
        }
    }

    if (it != m_views.End())
    {
        m_views.Erase(it);
    }
}

RenderStats* World::GetRenderStats() const
{
    if (!IsReady())
    {
        return nullptr;
    }

    return &const_cast<RenderStats&>(m_renderResource->GetRenderStats());
}

} // namespace hyperion
