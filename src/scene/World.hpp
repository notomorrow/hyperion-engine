#ifndef HYPERION_V2_WORLD_H
#define HYPERION_V2_WORLD_H

#include <scene/Scene.hpp>
#include <physics/PhysicsWorld.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <core/lib/AtomicSemaphore.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

using renderer::Frame;

class World : public EngineComponentBase<STUB_CLASS(World)>
{
public:
    World();
    World(const World &other) = delete;
    World &operator=(const World &other) = delete;
    ~World();

    Octree &GetOctree() { return m_octree; }
    const Octree &GetOctree() const { return m_octree; }

    PhysicsWorld &GetPhysicsWorld() { return m_physics_world; }
    const PhysicsWorld &GetPhysicsWorld() const { return m_physics_world; }

    void AddScene(const Handle<Scene> &scene);
    void AddScene(Handle<Scene> &&scene);
    void RemoveScene(ID<Scene> id);

    void Init();
    
    /*! \brief Perform any necessary game thread specific updates to the World.
     * The main logic loop of the engine happens here. Each Scene in the World is updated,
     * and within each Scene, each Entity, etc.
     */
    void Update(GameCounter::TickUnit delta);

    /*! \brief Perform any necessary render thread specific updates to the World. */
    void PreRender(Frame *frame);
    void Render(Frame *frame);

private:
    void PerformSceneUpdates();

    Octree m_octree;
    PhysicsWorld m_physics_world;

    FlatSet<Handle<Scene>> m_scenes;
    FlatSet<Handle<Scene>> m_scenes_pending_addition;
    FlatSet<Handle<Scene>> m_scenes_pending_removal;

    std::atomic_bool m_has_scene_updates { false };
    BinarySemaphore m_scene_update_sp;
    std::mutex m_scene_update_mutex;
};

} // namespace hyperion::v2

#endif
