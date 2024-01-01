#include <scene/controllers/ShadowMapController.hpp>
#include <scene/controllers/LightController.hpp>

#include <scene/Scene.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shadows.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

ShadowMapController::ShadowMapController()
    : Controller(true),
      m_shadow_map_renderer(nullptr)
{
}

ShadowMapController::ShadowMapController(Handle<Light> light)
    : Controller(true),
      m_light(std::move(light)),
      m_shadow_map_renderer(nullptr)
{
}

void ShadowMapController::AddShadowMapRenderer(const Handle<Scene> &scene)
{
    AssertThrow(scene.IsValid());

    if (scene->IsWorldScene()) {
        m_shadow_map_renderer_name = HYP_NAME(TEMP_ShadowMapRenderer0);
        m_shadow_map_renderer_scene = scene;

        m_shadow_map_renderer = scene->GetEnvironment()->AddRenderComponent<ShadowMapRenderer>(m_shadow_map_renderer_name);
    }

    UpdateShadowCamera(GetOwner()->GetTransform());
}

void ShadowMapController::RemoveShadowMapRenderer()
{
    if (!m_shadow_map_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = m_shadow_map_renderer_scene.Lock()) {
        scene->GetEnvironment()->RemoveRenderComponent<ShadowMapRenderer>(m_shadow_map_renderer_name);

        m_shadow_map_renderer = nullptr;
        m_shadow_map_renderer_name = Name::invalid;
        m_shadow_map_renderer_scene.Reset();
    }
}

void ShadowMapController::UpdateShadowCamera(const Transform &transform)
{
    if (!m_light || !m_shadow_map_renderer) {
        return;
    }

    // For directional lights

    const Vector3 &center = transform.GetTranslation();

    const Float radius = 15.0f;

    BoundingBox aabb = BoundingBox(center - radius, center + radius);

    const Vector3 light_direction = m_light->GetPosition().Normalized() * -1.0f;

    AssertThrow(m_shadow_map_renderer->GetPass() != nullptr);

    const Handle<Camera> &camera = m_shadow_map_renderer->GetPass()->GetCamera();

    if (!camera) {
        return;
    }

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);
    
    FixedArray<Vector3, 8> corners = aabb.GetCorners();

    for (Vector3 &corner : corners) {
        corner = camera->GetViewMatrix() * corner;

        aabb.max = MathUtil::Max(aabb.max, corner);
        aabb.min = MathUtil::Min(aabb.min, corner);
    }

    aabb.max.z = radius;
    aabb.min.z = -radius;

    m_shadow_map_renderer->SetCameraData(ShadowMapCameraData {
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix(),
        aabb
    });
}

void ShadowMapController::OnAttachedToScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (m_shadow_map_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        AddShadowMapRenderer(scene);
    }
}

void ShadowMapController::OnDetachedFromScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (id == m_shadow_map_renderer_scene.GetID()) {
        RemoveShadowMapRenderer();
    }
}

void ShadowMapController::OnAdded()
{
    Threads::AssertOnThread(THREAD_GAME);

    if (m_light) {
        // TEMP: Should refactor this so it is more able to be dynamically bound
        // and unbound, not dependent on a static index.
        m_light->SetShadowMapIndex(0);
    } else {
        DebugLog(
            LogType::Warn,
            "ShadowMapController has invalid Light\n"
        );
    }
}

void ShadowMapController::OnRemoved()
{
    Threads::AssertOnThread(THREAD_GAME);

    // OnDetachedFromScene() already handles removing the renderer


    if (m_light) {
        m_light->SetShadowMapIndex(~0u);
        m_light.Reset();
    }
}

void ShadowMapController::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
}

void ShadowMapController::OnTransformUpdate(const Transform &transform)
{
    Threads::AssertOnThread(THREAD_GAME);

    // Update camera data on transform change

    UpdateShadowCamera(transform);
}

} // namespace hyperion::v2