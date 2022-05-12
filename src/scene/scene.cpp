#include "scene.h"
#include "../engine.h"

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
      m_root_node(std::make_unique<Node>("root")), 
      m_shader_data_state(ShaderDataState::DIRTY)
{
}

Scene::~Scene()
{
    Teardown();
}
    
void Scene::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }
    
    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SCENES, [this](Engine *engine) {
        for (auto &texture : m_environment_textures) {
            if (texture != nullptr) {
                texture.Init();
            }
        }

        for (auto &light : m_lights) {
            if (light != nullptr) {
                light.Init();
            }
        }

        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            engine->render_scheduler.FlushOrWait([](auto &fn) {
                HYPERION_ASSERT_RESULT(fn());
            });
        }), engine);
    }));
}

void Scene::Update(Engine *engine, double delta_time)
{
    if (m_camera != nullptr) {
        m_camera->Update(delta_time);

        if (m_camera->GetViewProjectionMatrix() != m_last_view_projection_matrix) {
            m_last_view_projection_matrix = m_camera->GetViewProjectionMatrix();
            m_shader_data_state |= ShaderDataState::DIRTY;
        }
    }

    m_root_node->Update(engine);

    if (m_shader_data_state.IsDirty()) {
        UpdateShaderData(engine);
    }
}

void Scene::UpdateShaderData(Engine *engine)
{
    struct {
        BoundingBox aabb;
        Matrix4     view;
        Matrix4     projection;
        Vector3     translation;
        Vector3     direction;
        int         width;
        int         height;
        Vector3     light_direction;
    } params = {
        .aabb        = m_aabb,
        .light_direction = Vector3(-0.5f, 0.5f, 0.0f).Normalize()
    };

    if (m_camera != nullptr) {
        params.view        = m_camera->GetViewMatrix();
        params.projection  = m_camera->GetProjectionMatrix();
        params.translation = m_camera->GetTranslation();
        params.direction   = m_camera->GetDirection();
        params.width       = m_camera->GetWidth();
        params.height      = m_camera->GetHeight();
    }

    engine->render_scheduler.Enqueue([this, engine, params] {
        SceneShaderData shader_data{
            .view             = params.view,
            .projection       = params.projection,
            .camera_position  = params.translation.ToVector4(),
            .camera_direction = params.direction.ToVector4(),
            .light_direction  = params.light_direction.ToVector4(),
            .resolution_x     = static_cast<uint32_t>(params.width),
            .resolution_y     = static_cast<uint32_t>(params.height),
            .aabb_max         = params.aabb.max.ToVector4(),
            .aabb_min         = params.aabb.min.ToVector4()
        };

        shader_data.environment_texture_usage = 0;

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_environment_textures.size()); i++) {
            if (auto &texture = m_environment_textures[i]) {
                /* should we use an enqueued task for this? */
                if (engine->shader_globals->textures.GetResourceIndex(texture.ptr, &shader_data.environment_texture_index)) {
                    shader_data.environment_texture_usage |= 1 << i;
                }
            }
        }
        
        engine->shader_globals->scenes.Set(m_id.value - 1, shader_data);

        HYPERION_RETURN_OK;
    });

    m_shader_data_state = ShaderDataState::CLEAN;
}

void Scene::AddLight(Ref<Light> &&light)
{
    if (light != nullptr && IsInit()) {
        light.Init();
    }

    m_lights.push_back(std::move(light));
}

void Scene::SetEnvironmentTexture(uint32_t index, Ref<Texture> &&texture)
{
    if (texture && IsInit()) {
        texture.Init();
    }

    m_environment_textures[index] = std::move(texture);

    m_shader_data_state |= ShaderDataState::DIRTY;
}

} // namespace hyperion::v2