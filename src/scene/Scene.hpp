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
#include <rendering/EnvProbe.hpp>
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
    void SetCamera(Handle<Camera> &&camera);

    /*! \brief Add the Entity to a new Node attached to the root. */
    bool AddEntity(Handle<Entity> &&entity);
    /*! \brief Remove a Node from the Scene with the given Entity */
    bool RemoveEntity(const Handle<Entity> &entity);

    /*! \brief Add an Entity to the queue. On Update(), it will be added to the scene. */
    bool AddEntityInternal(Handle<Entity> &&entity);
    bool HasEntity(Entity::ID id) const;
    /*! \brief Add an Remove to the from the Scene in an enqueued way. On Update(), it will be removed from the scene. */
    bool RemoveEntityInternal(const Handle<Entity> &entity);

    bool AddLight(Handle<Light> &&light);
    bool AddLight(const Handle<Light> &light);
    bool RemoveLight(Light::ID id);

    bool AddEnvProbe(Handle<EnvProbe> &&env_probe);
    bool AddEnvProbe(const Handle<EnvProbe> &env_probe);
    bool RemoveEnvProbe(EnvProbe::ID id);

    /* ONLY CALL FROM GAME THREAD!!! */
    auto &GetEntities() { return m_entities; }
    const auto &GetEntities() const { return m_entities; }

    NodeProxy &GetRoot() { return m_root_node_proxy; }
    const NodeProxy &GetRoot() const { return m_root_node_proxy; }

    /*! \brief Used for deserialization only */
    void SetRoot(NodeProxy &&root)
    {
        if (m_root_node_proxy) {
            m_root_node_proxy.Get()->SetScene(nullptr);
        }

        m_root_node_proxy = std::move(root);

        if (m_root_node_proxy) {
            m_root_node_proxy.Get()->SetScene(this);
        }
    }

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

    // entities, lights etc. live in GAME thread
    FlatMap<IDBase, Handle<Entity>> m_entities;
    FlatMap<Light::ID, Handle<Light>> m_lights;
    FlatMap<EnvProbe::ID, Handle<EnvProbe>> m_env_probes;

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
