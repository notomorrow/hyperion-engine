#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "node.h"
#include "spatial.h"
#include "octree.h"
#include <rendering/base.h>
#include <rendering/texture.h>
#include <rendering/shader.h>
#include <rendering/light.h>
#include <core/scheduler.h>
#include <core/lib/flat_set.h>
#include <core/lib/flat_map.h>
#include <camera/camera.h>
#include <game_counter.h>
#include <types.h>
#include <unordered_map>

namespace hyperion::v2 {

class Environment;

class Scene : public EngineComponentBase<STUB_CLASS(Scene)> {
    friend class Spatial; // TODO: refactor to not need as many friend classes
public:
    static constexpr UInt32 max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(std::unique_ptr<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Camera *GetCamera() const                            { return m_camera.get(); }
    void SetCamera(std::unique_ptr<Camera> &&camera)     { m_camera = std::move(camera); }
    Octree &GetOctree()                                  { return m_octree; }
    const Octree &GetOctree() const                      { return m_octree; }

    bool AddSpatial(Ref<Spatial> &&spatial);
    bool HasSpatial(Spatial::ID id) const;
    bool RemoveSpatial(Spatial::ID id);
    bool RemoveSpatial(const Ref<Spatial> &spatial);

    /*! ONLY CALL FROM GAME THREAD!!! */
    auto &GetSpatials()                                  { return m_spatials; }
    const auto &GetSpatials() const                      { return m_spatials; }

    Node *GetRootNode() const                            { return m_root_node.get(); }

    Environment *GetEnvironment() const                  { return m_environment; }

    Texture *GetEnvironmentTexture(uint32_t index) const { return m_environment_textures[index].ptr; }
    void SetEnvironmentTexture(uint32_t index, Ref<Texture> &&texture);

    Scene::ID GetParentId() const                        { return m_parent_id; }
    void SetParentId(Scene::ID id)                       { m_parent_id = id; }
    
    void Init(Engine *engine);
    void Update(Engine *engine, GameCounter::TickUnit delta);

    BoundingBox m_aabb;

private:
    void EnqueueRenderUpdates(Engine *engine);

    void RequestPipelineChanges(Ref<Spatial> &spatial);
    void RemoveFromPipeline(Ref<Spatial> &spatial, GraphicsPipeline *pipeline);
    void RemoveFromPipelines(Ref<Spatial> &spatial);

    std::unique_ptr<Camera>  m_camera;
    std::unique_ptr<Node>    m_root_node;
    Octree                   m_octree;
    Environment             *m_environment;
    std::array<Ref<Texture>, max_environment_textures> m_environment_textures;

    // spatials live in GAME thread
    std::unordered_map<IDBase, Ref<Spatial>> m_spatials;

    Matrix4                 m_last_view_projection_matrix;

    ScheduledFunctionId     m_render_update_id;

    Scene::ID               m_parent_id;

    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif
