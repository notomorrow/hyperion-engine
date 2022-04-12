#include "scene.h"
#include "../engine.h"

namespace hyperion::v2 {

Scene::Scene(std::unique_ptr<Camera> &&camera)
    : EngineComponentBase(),
      m_camera(std::move(camera))
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
        UpdateShaderData(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SCENES, [this](Engine *engine) {
            /* no-op */
        }), engine);
    }));
}

void Scene::Update(Engine *engine, double delta_time)
{
    if (m_camera == nullptr) {
        return;
    }

    m_camera->Update(delta_time);

    UpdateShaderData(engine);
}

void Scene::UpdateShaderData(Engine *engine) const
{
    if (m_camera == nullptr) {
        return;
    }

    engine->shader_globals->scenes.Set(
        m_id.value - 1,
        {
            .view            = m_camera->GetViewMatrix(),
            .projection      = m_camera->GetProjectionMatrix(),
            .camera_position = Vector4(m_camera->GetTranslation(), 1.0f),
            .light_direction = Vector4(Vector3(0.5f, 0.5f, 0.0f).Normalize(), 1.0f) /* TODO */
        }
    );
}

} // namespace hyperion::v2