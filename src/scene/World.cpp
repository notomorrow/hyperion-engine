#include "World.hpp"
#include <Engine.hpp>
#include <Threads.hpp>

#include "rendering/RenderEnvironment.hpp"

namespace hyperion::v2 {

using renderer::RTUpdateStateFlags;

World::World()
    : EngineComponentBase(),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f)))
{
}

World::~World()
{
    Teardown();
}
    
void World::Init()
{
    if (IsInitCalled()) {
        return;
    }

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (Handle<Scene> &scene : m_scenes) {
        InitObject(scene);
    }
    
    EngineComponentBase::Init();

    m_physics_world.Init();

    SetReady(true);

    OnTeardown([this]() {
        for (Handle<Scene> &scene : m_scenes) {
            if (!scene) {
                continue;
            }

            scene->SetWorld(nullptr);
        }
        
        m_scenes.Clear();
        
        m_scene_update_mutex.lock();
        m_scenes_pending_addition.Clear();
        m_scenes_pending_removal.Clear();
        m_scene_update_mutex.unlock();

        m_physics_world.Teardown();
        
        SetReady(false);
    });
}

void World::PerformSceneUpdates()
{
    std::lock_guard guard(m_scene_update_mutex);

    for (Handle<Scene> &scene : m_scenes_pending_removal) {
        if (!scene) {
            continue;
        }

        const auto it = m_scenes.Find(scene);

        if (it == m_scenes.End()) {
            continue;
        }

        scene->SetWorld(nullptr);
        m_scenes.Erase(it);

        // remove scenes with that as parent
        for (auto other_it = m_scenes.Begin(); other_it != m_scenes.End();) {
            if ((*other_it)->GetParentScene() == scene) {
                other_it = m_scenes.Erase(other_it);
            } else {
                ++other_it;
            }
        }
    }

    m_scenes_pending_removal.Clear();

    for (Handle<Scene> &scene : m_scenes_pending_addition) {
        if (!scene) {
            continue;
        }

        scene->SetWorld(this);

        m_scenes.Insert(std::move(scene));
    }

    m_scenes_pending_addition.Clear();

    m_has_scene_updates.store(false);
}

void World::Update(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    m_octree.NextVisibilityState();

    for (Handle<Scene> &scene : m_scenes) {
        m_octree.CalculateVisibility(scene.Get());
    }

    m_physics_world.Tick(delta);

    if (m_has_scene_updates.load()) {
        PerformSceneUpdates();
    }

    for (Handle<Scene> &scene : m_scenes) {
        scene->Update(delta);
    }
}

void World::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    // set visibility cursor to previous Octree visibility cursor (atomic, relaxed)
    Engine::Get()->render_state.visibility_cursor = m_octree.LoadPreviousVisibilityCursor();
}

void World::AddScene(const Handle<Scene> &scene)
{
    AddScene(Handle<Scene>(scene));
}

void World::AddScene(Handle<Scene> &&scene)
{
    if (!scene) {
        return;
    }

    InitObject(scene);

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_addition.Insert(std::move(scene));

    m_has_scene_updates.store(true);
}

void World::RemoveScene(ID<Scene> id)
{
    if (!id) {
        return;
    }

    std::lock_guard guard(m_scene_update_mutex);

    m_scenes_pending_removal.Insert(Handle<Scene>(id));

    m_has_scene_updates.store(true);
}

} // namespace hyperion::v2
