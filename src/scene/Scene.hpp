#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "Node.hpp"
#include "NodeProxy.hpp"
#include "Entity.hpp"
#include "Octree.hpp"
#include <core/Base.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/FlatMap.hpp>
#include <camera/Camera.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>
#include <unordered_map>

namespace hyperion::v2 {

class RenderEnvironment;
class World;

class Scene : public EngineComponentBase<STUB_CLASS(Scene)>
{
    friend class Entity;
    friend class World;
public:
    static constexpr UInt32 max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(Handle<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Handle<Camera> &GetCamera() { return m_camera; }
    const Handle<Camera> &GetCamera() const { return m_camera; }
    void SetCamera(Handle<Camera> &&camera) { m_camera = std::move(camera); }

    /*! \brief Add an Entity to the queue. On Update(), it will be added to the scene. */
    bool AddEntity(Handle<Entity> &&entity);
    bool HasEntity(Entity::ID id) const;
    /*! \brief Add an Remove to the from the Scene in an enqueued way. On Update(), it will be removed from the scene. */
    bool RemoveEntity(const Handle<Entity> &entity);

    /* ONLY CALL FROM GAME THREAD!!! */
    auto &GetEntities() { return m_entities; }
    const auto &GetEntities() const { return m_entities; }

    NodeProxy &GetRoot() { return m_root_node_proxy; }
    const NodeProxy &GetRoot() const { return m_root_node_proxy; }

    RenderEnvironment *GetEnvironment() const { return m_environment; }

    World *GetWorld() const { return m_world; }
    void SetWorld(World *world);

    Scene::ID GetParentID() const { return m_parent_id; }
    void SetParentID(Scene::ID id) { m_parent_id = id; }

    /*! \brief A scene is a "virtual scene" if it exists not as an owner of entities,
        but rather a simple container that has items based on another Scene. For example,
        you could have a "shadow map" scene, which gathers entities from the main scene,
        but does not call Update() on them. */
    bool IsVirtualScene() const { return m_parent_id != Scene::empty_id; }
    
    void Init(Engine *engine);

    void ForceUpdate();

    BoundingBox m_aabb;

private:
    // World only calls
    void Update(
        Engine *engine,
        GameCounter::TickUnit delta,
        bool update_octree_visiblity = true
    );

    void EnqueueRenderUpdates();
    
    void AddPendingEntities();
    void RemovePendingEntities();

    void RequestRendererInstanceUpdate(Handle<Entity> &entity);
    void RemoveFromRendererInstance(Handle<Entity> &entity, RendererInstance *renderer_instance);
    void RemoveFromRendererInstances(Handle<Entity> &entity);

    Handle<Camera> m_camera;
    NodeProxy m_root_node_proxy;
    RenderEnvironment *m_environment;
    World *m_world;

    // entities live in GAME thread
    FlatMap<IDBase, Handle<Entity>> m_entities;

    // NOTE: not for thread safety, it's to defer updates so we don't
    // remove in the update loop.
    FlatSet<Entity::ID> m_entities_pending_removal;
    FlatSet<Handle<Entity>> m_entities_pending_addition;

    Matrix4 m_last_view_projection_matrix;
                                 
    Scene::ID m_parent_id;
                                 
    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
