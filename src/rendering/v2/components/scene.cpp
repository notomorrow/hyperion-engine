#include "scene.h"
#include "../engine.h"

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera)),
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

        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            /* no-op */
        }), engine);
    }));
}

void Scene::Update(Engine *engine, double delta_time)
{
    if (m_camera != nullptr) {
        m_camera->Update(delta_time);

        m_shader_data_state |= ShaderDataState::DIRTY;
    }

    if (m_shader_data_state.IsDirty()) {
        UpdateShaderData(engine);
    }
}

void Scene::UpdateShaderData(Engine *engine) const
{
    SceneShaderData shader_data{};

    if (m_camera != nullptr) {
        shader_data.view            = m_camera->GetViewMatrix();
        shader_data.projection      = m_camera->GetProjectionMatrix();
        shader_data.camera_position = Vector4(m_camera->GetTranslation(), 1.0f);
        shader_data.resolution_x    = static_cast<uint32_t>(m_camera->GetWidth());
        shader_data.resolution_y    = static_cast<uint32_t>(m_camera->GetHeight());
    }

    shader_data.light_direction = Vector4(Vector3(0.5f, 1.0f, 0.0f).Normalize(), 1.0f);
    shader_data.environment_texture_usage = 0;

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_environment_textures.size()); i++) {
        if (auto &texture = m_environment_textures[i]) {
            if (engine->shader_globals->textures.GetResourceIndex(texture.ptr, &shader_data.environment_texture_index)) {
                shader_data.environment_texture_usage |= 1 << i;
            }
        }
    }
    
    engine->shader_globals->scenes.Set(m_id.value - 1, std::move(shader_data));

    m_shader_data_state = ShaderDataState::CLEAN;
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