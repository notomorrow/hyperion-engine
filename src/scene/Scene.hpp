#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "Node.hpp"
#include "NodeProxy.hpp"
#include "Entity.hpp"
#include "Octree.hpp"
#include <core/Base.hpp>
#include <core/Scheduler.hpp>
#include <core/Containers.hpp>
#include <core/Name.hpp>
#include <rendering/rt/TLAS.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Light.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <scene/camera/Camera.hpp>
#include <math/Color.hpp>
#include <GameCounter.hpp>
#include <Types.hpp>
#include <unordered_map>

namespace hyperion::v2 {

class RenderEnvironment;
class World;
class Scene;

struct FogParams
{
    Color color = Color(0xF2F8F7FF);
    Float start_distance = 250.0f;
    Float end_distance = 1000.0f;
};

template<>
struct ComponentInitInfo<STUB_CLASS(Scene)>
{
    enum Flags : ComponentFlags
    {
        SCENE_FLAGS_NONE = 0x0,
        SCENE_FLAGS_HAS_TLAS = 0x1
    };

    ComponentFlags flags = SCENE_FLAGS_NONE;
};

class Scene
    : public EngineComponentBase<STUB_CLASS(Scene)>,
      public HasDrawProxy<STUB_CLASS(Scene)>
{
    friend class Entity;
    friend class World;
    friend class UIScene;

public:
    Scene();
    Scene(Handle<Camera> &&camera);

    Scene(
        Handle<Camera> &&camera,
        const InitInfo &info
    );

    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Handle<Camera> &GetCamera()
        { return m_camera; }

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    void SetCamera(Handle<Camera> &&camera);

    RenderList &GetRenderList()
        { return m_render_list; }

    const RenderList &GetRenderList() const
        { return m_render_list; }

    /*! \brief Add the Entity to a new Node attached to the root.
     * @returns If successfully added, returns the NodeProxy which the Entity was attached to. Otherwise, returns an empty NodeProxy.
     */
    NodeProxy AddEntity(Handle<Entity> &&entity);
    /*! \brief Add the Entity to a new Node attached to the root.
     * @returns If successfully added, returns the NodeProxy which the Entity was attached to. Otherwise, returns an empty NodeProxy.
     * */
    NodeProxy AddEntity(const Handle<Entity> &entity)
        { return AddEntity(Handle<Entity>(entity)); }

    /*! \brief Remove a Node from the Scene with the given Entity */
    bool RemoveEntity(ID<Entity> entity_id);
    /*! \brief Remove a Node from the Scene with the given Entity */
    bool RemoveEntity(const Handle<Entity> &entity)
        { return RemoveEntity(entity.GetID()); }

    /*! \brief Add an Entity to the queue. On Update(), it will be added to the scene. */
    bool AddEntityInternal(Handle<Entity> &&entity);
    bool HasEntity(ID<Entity> entity_id) const;
    /*! \brief Add an Remove to the from the Scene in an enqueued way. On Update(), it will be removed from the scene. */
    bool RemoveEntityInternal(const Handle<Entity> &entity);

    const Handle<Entity> &FindEntityWithID(ID<Entity>) const;
    const Handle<Entity> &FindEntityByName(Name) const;

    bool AddLight(Handle<Light> &&light);
    bool AddLight(const Handle<Light> &light);
    bool RemoveLight(ID<Light> id);

    bool AddEnvProbe(Handle<EnvProbe> &&env_probe);
    bool AddEnvProbe(const Handle<EnvProbe> &env_probe);
    bool RemoveEnvProbe(ID<EnvProbe> id);

    FogParams &GetFogParams()
        { return m_fog_params; }

    const FogParams &GetFogParams() const
        { return m_fog_params; }

    void SetFogParams(const FogParams &fog_params)
        { m_fog_params = fog_params; }

    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    Handle<TLAS> &GetTLAS()
        { return m_tlas; }
    
    /*! \brief Get the top level acceleration structure for this Scene, if it exists. */
    const Handle<TLAS> &GetTLAS() const
        { return m_tlas; }

    /*! \brief Creates a top level acceleration structure for this Scene. If one already exists on this Scene,
     *  no action is performed and true is returned. If the TLAS could not be created, false is returned.
     *  The Scene must have had Init() called on it before calling this.
     */
    bool CreateTLAS();

    /* ONLY USE FROM GAME THREAD */
    auto &GetEntities()
        { return m_entities; }

    const auto &GetEntities() const
        { return m_entities; }

    NodeProxy &GetRoot()
        { return m_root_node_proxy; }

    const NodeProxy &GetRoot() const
        { return m_root_node_proxy; }

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

    RenderEnvironment *GetEnvironment() const
        { return m_environment.Get(); }

    World *GetWorld() const
        { return m_world; }

    void SetWorld(World *world);

    /*! \brief A scene is a non-world scene if it exists not as an owner of entities,
        but rather a simple container that has items based on another Scene. For example,
        you could have a "shadow map" scene, which gathers entities from the main scene,
        but does not call Update() on them. */
    bool IsWorldScene() const
        { return !m_parent_scene.IsValid() && !m_is_non_world_scene; }
    
    void Init();

    void ForceUpdate();

    void CollectEntities(
        RenderList &render_list, 
        const Handle<Camera> &camera,
        const Bitset &bucket_bits,
        Optional<RenderableAttributeSet> override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

    void CollectEntities(
        RenderList &render_list, 
        const Handle<Camera> &camera,
        Optional<RenderableAttributeSet> override_attributes = { },
        bool skip_frustum_culling = false
    ) const;

private:
    // World only calls
    void Update(GameCounter::TickUnit delta);

    void EnqueueRenderUpdates();
    
    void AddPendingEntities();
    void RemovePendingEntities();

    bool IsEntityInFrustum(const Handle<Entity> &entity, ID<Camera> camera_id, UInt8 visibility_cursor) const;

    Handle<Camera> m_camera;
    RenderList m_render_list;

    NodeProxy m_root_node_proxy;
    UniquePtr<RenderEnvironment> m_environment;
    World *m_world;

    FogParams m_fog_params;

    // entities, lights etc. live in GAME thread
    FlatMap<ID<Entity>, Handle<Entity>> m_entities;
    FlatMap<ID<Light>, Handle<Light>> m_lights;
    FlatMap<ID<EnvProbe>, Handle<EnvProbe>> m_env_probes;

    // NOTE: not for thread safety, it's to defer updates so we don't
    // remove in the update loop.
    FlatSet<ID<Entity>> m_entities_pending_removal;
    FlatSet<Handle<Entity>> m_entities_pending_addition;


    Handle<TLAS> m_tlas;

    Matrix4 m_last_view_projection_matrix;

    Handle<Scene> m_parent_scene;
    bool m_is_non_world_scene;
                                 
    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
