#include "pssm_shadow_mapping.h"
#include "../shader_manager.h"
#include "../environment.h"
#include "../../util.h"

namespace hyperion {
PssmShadowMapping::PssmShadowMapping(int num_splits, double max_dist)
    : Renderable(fbom::FBOMObjectType("PSSM_SHADOW_MAPPING")),
      m_num_splits(num_splits),
      m_max_dist(max_dist),
      m_is_variance_shadow_mapping(ShaderManager::GetInstance()->GetBaseShaderProperties().GetValue("SHADOWS_VARIANCE").IsTruthy())
{
    m_shadow_renderers.resize(num_splits, nullptr);

    Environment::GetInstance()->SetNumCascades(num_splits);

    double dist = max_dist;

    for (int i = num_splits - 1; i >= 0; i--) {
        dist /= 2;
        // const double frac = double(i + 1) / double(num_splits);
        // const double distance = MathUtil::Lerp(min_dist, max_dist, frac);
        // const double distance = max_dist * frac;
        m_shadow_renderers[i] = std::make_shared<ShadowMapping>(dist, i);
    }
}

int PssmShadowMapping::NumSplits() const
{
    return m_num_splits;
}

void PssmShadowMapping::SetLightDirection(const Vector3 &dir)
{
    for (int i = 0; i < m_num_splits; i++) {
        m_shadow_renderers[i]->SetLightDirection(dir);
    }
}

void PssmShadowMapping::SetOrigin(const Vector3 &origin)
{
    m_origin = origin;

    for (int i = 0; i < m_num_splits; i++) {
        m_shadow_renderers[i]->SetOrigin(origin);
    }
}

void PssmShadowMapping::SetVarianceShadowMapping(bool value)
{
    if (value == m_is_variance_shadow_mapping) {
        return;
    }

    for (int i = 0; i < m_num_splits; i++) {
        m_shadow_renderers[i]->SetVarianceShadowMapping(value);
    }

    m_is_variance_shadow_mapping = value;
}

void PssmShadowMapping::Render(Renderer *renderer, Camera *cam)
{
    if (!renderer->GetEnvironment()->PSSMEnabled()) {
        return;
    }

    for (int i = 0; i < m_num_splits; i++) {
        m_shadow_renderers[i]->Render(renderer, cam);
    }
}

std::shared_ptr<Renderable> PssmShadowMapping::CloneImpl()
{
    return std::make_shared<PssmShadowMapping>(m_num_splits, m_max_dist);
}
} // namespace hyperion
