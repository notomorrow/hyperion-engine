#include "scene.h"
#include <engine.h>
#include <rendering/environment.h>

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node(std::make_unique<Node>("root")),
      m_octree(BoundingBox(Vector3(-250.0f), Vector3(250.0f))),
      m_environment(new Environment),
      m_shader_data_state(ShaderDataState::DIRTY)
{
    m_root_node->SetScene(this);
}

Scene::~Scene()
{
    m_root_node->SetScene(nullptr);

    Teardown();

    delete m_environment;
}
    
void Scene::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }
    
    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](Engine *engine) {
        for (auto &texture : m_environment_textures) {
            if (texture != nullptr) {
                texture.Init();
            }
        }

        m_environment->Init(engine);

        SetReady(true);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            DebugLog(LogType::Debug, "Destroy scene #%lu\t%p\n", m_id.value, (void *)this);

            delete m_environment;
            m_environment = nullptr;

            HYP_FLUSH_RENDER_QUEUE(engine);

            SetReady(false);
        }), engine);
    }));
}

bool Scene::AddSpatial(Ref<Spatial> &&spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    auto it = m_spatials.Find(spatial->GetId());

    if (it != m_spatials.end()) {
        DebugLog(
            LogType::Warn,
            "Spatial #%u already exists in Scene #%u\n",
            spatial->GetId().value,
            m_id.value
        );

        return false;
    }

    spatial->m_scene = this;

    if (spatial->IsRenderable() && !spatial->GetPrimaryPipeline()) {
        if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
            if (!spatial->GetPipelines().Contains(pipeline.ptr)) {
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

            return false;
        }
    }

    // just here for now, will move into this class
    if (spatial->m_octree == nullptr) {
        spatial->AddToOctree(GetEngine(), m_octree);
    }

    m_spatials.Insert(spatial->GetId(), std::move(spatial));

    return true;
}

bool Scene::HasSpatial(Spatial::ID id) const
{
    Threads::AssertOnThread(THREAD_GAME);

    return m_spatials.Contains(id);
}

bool Scene::RemoveSpatial(Spatial::ID id)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();

    auto it = m_spatials.Find(id);
    if (it == m_spatials.End()) {
        return false;
    }

    auto &found_spatial = it->second;
    AssertThrow(found_spatial != nullptr);

    found_spatial->RemoveFromPipelines();

    if (found_spatial->m_octree != nullptr) {
        found_spatial->RemoveFromOctree(GetEngine());
    }

    m_spatials.Erase(it);

    return true;
}

bool Scene::RemoveSpatial(const Ref<Spatial> &spatial)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    AssertThrow(spatial != nullptr);

    auto it = m_spatials.Find(spatial->GetId());
    if (it == m_spatials.End()) {
        return false;
    }

    auto &found_spatial = it->second;

    found_spatial->RemoveFromPipelines();

    if (found_spatial->m_octree != nullptr) {
        found_spatial->RemoveFromOctree(GetEngine());
    }

    m_spatials.Erase(it);

    return true;
}

void Scene::Update(Engine *engine, GameCounter::TickUnit delta)
{
    AssertReady();

    m_environment->Update(engine, delta);

    if (m_camera != nullptr) {
        m_camera->Update(delta);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    for (auto &it : m_spatials) {
        auto &spatial = it.second;

        spatial->Update(engine, delta);

        if (spatial->m_primary_pipeline.changed) {
            RequestPipelineChanges(spatial);
        }
    }

    m_octree.CalculateVisibility(this);

    EnqueueRenderUpdates(engine);
}

void Scene::RequestPipelineChanges(Ref<Spatial> &spatial)
{
    AssertThrow(spatial != nullptr);
    AssertThrow(spatial->m_scene == this);

    if (spatial->m_primary_pipeline.pipeline != nullptr) {
        spatial->RemoveFromPipeline(GetEngine(), spatial->m_primary_pipeline.pipeline);
    }

    if (!spatial->GetPrimaryPipeline()) {
        if (auto pipeline = GetEngine()->FindOrCreateGraphicsPipeline(spatial->GetRenderableAttributes())) {
            if (!spatial->GetPipelines().Contains(pipeline.ptr)) {
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

void Scene::EnqueueRenderUpdates(Engine *engine)
{
    struct {
        BoundingBox aabb;
        Matrix4     view;
        Matrix4     projection;
        Vector3     translation;
        Vector3     direction;
        Int32       width;
        Int32       height;
        Vector3     light_direction;
        Float32     global_timer;
    } params = {
        .aabb            = m_aabb,
        .light_direction = Vector3(-0.5f, 0.5f, 0.0f).Normalize(),
        .global_timer    = m_environment->GetGlobalTimer()
    };

    if (m_camera != nullptr) {
        params.view        = m_camera->GetViewMatrix();
        params.projection  = m_camera->GetProjectionMatrix();
        params.translation = m_camera->GetTranslation();
        params.direction   = m_camera->GetDirection();
        params.width       = m_camera->GetWidth();
        params.height      = m_camera->GetHeight();
    }

    engine->render_scheduler.EnqueueReplace(m_render_update_id, [this, engine, params](...) {
        SceneShaderData shader_data{
            .view                        = params.view,
            .projection                  = params.projection,
            .camera_position             = params.translation.ToVector4(),
            .camera_direction            = params.direction.ToVector4(),
            .light_direction             = params.light_direction.ToVector4(),
            .resolution_x                = static_cast<UInt32>(params.width),
            .resolution_y                = static_cast<UInt32>(params.height),
            .aabb_max                    = params.aabb.max.ToVector4(),
            .aabb_min                    = params.aabb.min.ToVector4(),
            .global_timer                = params.global_timer,
            .num_environment_shadow_maps = static_cast<UInt32>(m_environment->HasRenderComponent<ShadowRenderer>()) // callable on render thread only
        };

        shader_data.environment_texture_usage = 0;

        for (UInt i = 0; i < static_cast<UInt>(m_environment_textures.size()); i++) {
            if (auto &texture = m_environment_textures[i]) {
                shader_data.environment_texture_index = texture->GetId().value - 1;
                shader_data.environment_texture_usage |= 1u << i;
            }
        }
        
        engine->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Scene::SetEnvironmentTexture(UInt32 index, Ref<Texture> &&texture)
{
    if (texture && IsReady()) {
        texture.Init();
    }

    m_environment_textures[index] = std::move(texture);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

} // namespace hyperion::v2
