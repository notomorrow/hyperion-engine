#include "environment.h"
#include "shader_manager.h"

#include <algorithm>

namespace hyperion {
Environment *Environment::instance = nullptr;

const Vector2 Environment::possion_disk[16] = {
    Vector2(-0.94201624f, -0.39906216f),
    Vector2(0.94558609f, -0.76890725f),
    Vector2(-0.094184101f, -0.92938870f),
    Vector2(0.34495938f, 0.29387760f),
    Vector2(-0.91588581f, 0.45771432f),
    Vector2(-0.81544232f, -0.87912464f),
    Vector2(-0.38277543f, 0.27676845f),
    Vector2(0.97484398f, 0.75648379f),
    Vector2(0.44323325f, -0.97511554f),
    Vector2(0.53742981f, -0.47373420f),
    Vector2(-0.26496911f, -0.41893023f),
    Vector2(0.79197514f, 0.19090188f),
    Vector2(-0.24188840f, 0.99706507f),
    Vector2(-0.81409955f, 0.91437590f),
    Vector2(0.19984126f, 0.78641367f),
    Vector2(0.14383161f, -0.14100790f)
};

const size_t Environment::max_point_lights_on_screen = 32;

Environment *Environment::GetInstance()
{
    if (instance == nullptr) {
        instance = new Environment();
    }

    return instance;
}

Environment::Environment()
    : m_gravity(0.0f, -9.81f, 0.0f),
      m_shadows_enabled(false),
      m_pssm_enabled(false),
      m_num_cascades(1),
      m_shadow_maps({ nullptr }),
      m_shadow_matrices({ Matrix4::Identity() }),
      m_sun(Vector3(-1, -1, -1).Normalize(), Vector4(0.9, 0.8, 0.7, 1.0)),
      m_probe_manager(ProbeManager::GetInstance())
{
}

Environment::~Environment()
{
}

void Environment::SetShadowsEnabled(bool shadows_enabled)
{
    if (shadows_enabled == m_shadows_enabled) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("SHADOWS", shadows_enabled)
    );

    if (!shadows_enabled) {
        SetPSSMEnabled(false);
    }

    m_shadows_enabled = shadows_enabled;
}

void Environment::SetPSSMEnabled(bool pssm_enabled)
{
    if (pssm_enabled == m_pssm_enabled) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("PSSM_ENABLED", pssm_enabled)
    );

    if (pssm_enabled) {
        SetShadowsEnabled(true);
    }

    m_pssm_enabled = pssm_enabled;
}

void Environment::SetNumCascades(int num_cascades)
{
    if (num_cascades == m_num_cascades) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("NUM_SPLITS", num_cascades)
    );

    m_num_cascades = num_cascades;
}

void Environment::CollectVisiblePointLights(Camera *camera)
{
    const Frustum &frustum = camera->GetFrustum();

    m_point_lights_sorted.clear();
    m_point_lights_sorted.reserve(m_point_lights.size());

    for (size_t i = 0; i < m_point_lights.size(); i++) {
        const auto &point_light = m_point_lights[i];

        // NOTE: we could also maybe just do a simple "behind camera" check?
        // also, if the frustum route does work then maybe it makes sense to refactor lights into being
        // some sorta Renderable?
        if (point_light == nullptr || !frustum.BoundingBoxInFrustum(point_light->GetAABB())) {
            continue;
        }

        m_point_lights_sorted.push_back(PointLightCameraData{
            point_light.get(),
            camera->GetTranslation().Distance(point_light->GetPosition())
        });
    }

    if (m_point_lights.size() > max_point_lights_on_screen) {
        std::sort(m_point_lights_sorted.begin(), m_point_lights_sorted.end(), [](const auto &a, const auto &b) {
            return a.distance < b.distance;
        });
    }
}

void Environment::BindLights(Shader *shader) const
{
    size_t num_visible_point_lights = NumVisiblePointLights();

    // TODO
    //shader->SetUniform("env_NumPointLights", int(num_visible_point_lights));

    for (int i = 0; i < num_visible_point_lights; i++) {
        if (PointLight *point_light = m_point_lights_sorted[i].point_light) {
            point_light->Bind(i, shader);
        }
    }
}

} // namespace hyperion
