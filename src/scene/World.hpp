#ifndef HYPERION_V2_WORLD_H
#define HYPERION_V2_WORLD_H

#include <scene/Scene.hpp>
#include <physics/PhysicsWorld.hpp>

namespace hyperion::v2 {

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

    FlatMap<Scene::ID, Handle<Scene>> &GetScenes() { return m_scenes; }
    const FlatMap<Scene::ID, Handle<Scene>> &GetScenes() const { return m_scenes; }

    void AddScene(Handle<Scene> &&scene);
    void RemoveScene(Scene::ID scene_id);

    void Init(Engine *engine);
    void Update(
        Engine *engine,
        GameCounter::TickUnit delta
    );

private:
    Octree m_octree;
    PhysicsWorld m_physics_world;
    FlatMap<Scene::ID, Handle<Scene>> m_scenes;
};

} // namespace hyperion::v2

#endif
