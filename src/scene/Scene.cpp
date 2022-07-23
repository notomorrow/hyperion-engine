#include "Scene.hpp"
#include <Engine.hpp>
#include <rendering/Environment.hpp>
#include <rendering/render_components/CubemapRenderer.hpp>

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node(std::make_unique<Node>("root")),
      m_environment(new Environment(this)),
      m_world(nullptr),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    m_root_node->SetScene(this);
}

Scene::~Scene()
{
    Teardown();

    delete m_environment;
}
    
void Scene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](...) {
        auto *engine = GetEngine();

        m_environment->Init(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](...) {
            auto *engine = GetEngine();

            m_root_node->SetScene(nullptr);

            for (auto &it : m_spatials) {
                AssertThrow(it.second != nullptr);

                it.second->SetScene(nullptr);

                RemoveFromPipelines(it.second);

                if (it.second->m_octree != nullptr) {
                    it.second->RemoveFromOctree(GetEngine());
                }
            }

            m_spatials.Clear();

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }));
    }));
}

void Scene::SetWorld(World *world)
{
    Threads::AssertOnThread(THREAD_GAME);

    for (auto &it : m_spatials) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        if (entity->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove spatial #%u from octree\n",
                entity->GetId().value
            );

            entity->RemoveFromOctree(GetEngine());
        }
    }

    m_world = world;

    if (m_world == nullptr) {
        return;
    }

    for (auto &it : m_spatials) {
        auto &entity = it.second;

        if (entity == nullptr) {
            continue;
        }

        AssertThrow(entity->m_octree == nullptr);
        
        DebugLog(
            LogType::Debug,
            "Add spatial #%u to octree\n",
            entity->GetId().value
        );

        entity->AddToOctree(GetEngine(), m_world->GetOctree());
    }
}

bool Scene::AddSpatial(Ref<Spatial> &&spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    if (m_spatials.Contains(spatial->GetId())) {
        DebugLog(
            LogType::Warn,
            "Spatial #%u already exists in Scene #%u\n",
            spatial->GetId().value,
            m_id.value
        );

        return false;
    }

    spatial->SetScene(this);
    spatial.Init();
    m_environment->OnEntityAdded(spatial);

    // // m_spatials.Insert(spatial->GetId(), std::move(spatial));
    // m_spatials.Insert(static_cast<IDBase>(spatial->GetId()), std::move(spatial));

    m_spatials_pending_addition.Insert(std::move(spatial));

    return true;
}

bool Scene::HasSpatial(Spatial::ID id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_spatials.Find(id) != m_spatials.End();
}

bool Scene::RemoveSpatial(const Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    auto it = m_spatials.Find(spatial->GetId());

    if (it == m_spatials.End()) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: not found\n",
            spatial->GetId().value
        );

        return false;
    }

    if (m_spatials_pending_removal.Contains(spatial->GetId())) {
        DebugLog(
            LogType::Warn,
            "Could not remove entity with id #%u: already pending removal\n",
            spatial->GetId().value
        );

        return false;
    }

    m_environment->OnEntityRemoved(it->second);

    m_spatials_pending_removal.Insert(spatial->GetId());

    return true;
}

void Scene::AddPendingEntities()
{
    if (m_spatials_pending_addition.Empty()) {
        return;
    }

    for (auto &spatial : m_spatials_pending_addition) {
        const auto id = spatial->GetId();

        if (spatial->IsRenderable() && !spatial->GetPrimaryRendererInstance()) {
            if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
                if (!spatial->GetRendererInstances().Contains(pipeline.ptr)) {
                    pipeline->AddSpatial(spatial.IncRef());

                    spatial->EnqueueRenderUpdates();
                }

                spatial->m_primary_pipeline = {
                    .pipeline = pipeline.ptr,
                    .changed  = false
                };
            } else {
                DebugLog(
                    LogType::Error,
                    "Could not find or create optimal graphics pipeline for Spatial #%lu!\n",
                    spatial->GetId().value
                );

                continue;
            }
        }

        if (spatial->m_octree == nullptr) {
            if (m_world != nullptr) {
                DebugLog(
                    LogType::Debug,
                    "Add spatial #%u to octree\n",
                    spatial->GetId().value
                );
                spatial->AddToOctree(GetEngine(), m_world->GetOctree());
            }
        }

        m_spatials.Insert(static_cast<IDBase>(id), std::move(spatial));
    }

    m_spatials_pending_addition.Clear();
}

void Scene::RemovePendingEntities()
{
    if (m_spatials_pending_removal.Empty()) {
        return;
    }

    for (auto &id : m_spatials_pending_removal) {
        auto it = m_spatials.Find(id);

        auto &found_spatial = it->second;
        AssertThrow(found_spatial != nullptr);

        RemoveFromPipelines(found_spatial);

        if (found_spatial->m_octree != nullptr) {
            DebugLog(
                LogType::Debug,
                "[scene] Remove spatial #%u from octree\n",
                found_spatial->GetId().value
            );

            found_spatial->RemoveFromOctree(GetEngine());
        }

        m_spatials.Erase(it);
    }

    m_spatials_pending_removal.Clear();
}

void Scene::Update(
    Engine *engine,
    GameCounter::TickUnit delta,
    bool update_octree_visibility
)
{
    AssertReady();

    if (!m_spatials_pending_addition.Empty()) {
        AddPendingEntities();
    }

    if (!m_spatials_pending_removal.Empty()) {
        RemovePendingEntities();
    }

    m_environment->Update(engine, delta);

    if (m_camera != nullptr) {
        m_camera->Update(engine, delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    EnqueueRenderUpdates();

    for (auto &it : m_spatials) {
        auto &entity = it.second;
        AssertThrow(entity != nullptr);

        entity->Update(engine, delta);
        AssertThrow(entity != nullptr);

        if (entity->m_primary_pipeline.changed) {
            RequestPipelineChanges(entity);
        }
    }

    if (m_world != nullptr && update_octree_visibility) {
        m_world->GetOctree().CalculateVisibility(this);
    }
}

void Scene::RequestPipelineChanges(Ref<Spatial> &spatial)
{
    AssertThrow(spatial != nullptr);
    AssertThrow(spatial->GetScene() == this);

    if (spatial->m_primary_pipeline.pipeline != nullptr) {
        RemoveFromPipeline(spatial, spatial->m_primary_pipeline.pipeline);
    }

    if (!spatial->GetPrimaryRendererInstance()) {
        if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
            if (!spatial->GetRendererInstances().Contains(pipeline.ptr)) {
                pipeline->AddSpatial(spatial.IncRef());
            }
            
            // TODO: wrapper function
            spatial->m_primary_pipeline = {
                .pipeline = pipeline.ptr,
                .changed  = false
            };
        } else {
            DebugLog(
                LogType::Error,
                "Could not find or create optimal graphics pipeline for Spatial #%lu!\n",
                spatial->GetId().value
            );
        }
    }
}

void Scene::RemoveFromPipeline(Ref<Spatial> &spatial, GraphicsPipeline *pipeline)
{
    pipeline->RemoveSpatial(spatial.IncRef());
}

void Scene::RemoveFromPipelines(Ref<Spatial> &spatial)
{
    auto pipelines = spatial->m_pipelines;

    for (auto *pipeline : pipelines) {
        if (pipeline == nullptr) {
            continue;
        }

        // have to inc ref so it can hold it in a temporary container
        pipeline->RemoveSpatial(spatial.IncRef());
    }
}

void Scene::EnqueueRenderUpdates()
{
    struct {
        BoundingBox aabb;
        Matrix4     view;
        Matrix4     projection;
        Float       camera_near;
        Float       camera_far;
        Vector3     translation;
        Int         width;
        Int         height;
        Float       global_timer;
    } params = {
        .aabb            = m_aabb,
        .global_timer    = m_environment->GetGlobalTimer()
    };

    if (m_camera != nullptr) {
        params.view        = m_camera->GetViewMatrix();
        params.projection  = m_camera->GetProjectionMatrix();
        params.camera_near = m_camera->GetNear();
        params.camera_far  = m_camera->GetFar();
        params.translation = m_camera->GetTranslation();
        params.width       = m_camera->GetWidth();
        params.height      = m_camera->GetHeight();
    }

    GetEngine()->render_scheduler.Enqueue([this, params](...) {
        SceneShaderData shader_data {
            .aabb_max                    = params.aabb.max.ToVector4(),
            .aabb_min                    = params.aabb.min.ToVector4(),
            .global_timer                = params.global_timer,
            .num_environment_shadow_maps = static_cast<UInt32>(m_environment->HasRenderComponent<ShadowRenderer>()), // callable on render thread only
            .num_lights                  = static_cast<UInt32>(m_environment->NumLights())
        };

        if (m_camera != nullptr) {
            shader_data.view            = m_camera->GetDrawProxy().view;
            shader_data.projection      = m_camera->GetDrawProxy().projection;
            shader_data.camera_position = m_camera->GetDrawProxy().position.ToVector4();
            shader_data.camera_near     = m_camera->GetDrawProxy().clip_near;
            shader_data.camera_far      = m_camera->GetDrawProxy().clip_far;
            shader_data.resolution_x    = m_camera->GetDrawProxy().dimensions.width;
            shader_data.resolution_y    = m_camera->GetDrawProxy().dimensions.height;
        }

        shader_data.environment_texture_usage = 0;

        //! TODO: allow any cubemap to be set on env
        if (const auto *cubemap_renderer = m_environment->GetRenderComponent<CubemapRenderer>()) {
            shader_data.environment_texture_index = cubemap_renderer->GetComponentIndex();
            shader_data.environment_texture_usage |= 1u << cubemap_renderer->GetComponentIndex();
        }

        // DebugLog(LogType::Debug, "set %u lights\n", shader_data.num_lights);
        
        GetEngine()->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

} // namespace hyperion::v2
