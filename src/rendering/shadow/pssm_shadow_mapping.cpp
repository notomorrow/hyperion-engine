#include "pssm_shadow_mapping.h"
#include "../shader_manager.h"
#include "../shaders/depth_shader.h"
#include "../environment.h"
#include "../../util.h"

namespace hyperion {
PssmShadowMapping::PssmShadowMapping(Camera *view_cam, int num_splits, double max_dist)
    : num_splits(num_splits)
{
    shadow_renderers.resize(num_splits, nullptr);

    Environment::GetInstance()->SetNumCascades(num_splits);

    // const double min_dist = max_dist / (double)num_splits;
    double dist = max_dist;

    for (int i = num_splits - 1; i >= 0; i--) {
        dist /= 2;
        // const double frac = double(i + 1) / double(num_splits);
        // const double distance = MathUtil::Lerp(min_dist, max_dist, frac);
        // const double distance = max_dist * frac;
        Environment::GetInstance()->SetShadowSplit(i, dist);
        shadow_renderers[i] = std::make_shared<ShadowMapping>(view_cam, dist);

        Environment::GetInstance()->SetShadowMap(i, shadow_renderers[i]->GetShadowMap());
    }

    m_depth_shader = ShaderManager::GetInstance()->GetShader<DepthShader>(ShaderProperties());
}

int PssmShadowMapping::NumSplits() const
{
    return num_splits;
}

void PssmShadowMapping::SetLightDirection(const Vector3 &dir)
{
    for (int i = 0; i < num_splits; i++) {
        shadow_renderers[i]->SetLightDirection(dir);
    }
}

void PssmShadowMapping::SetOrigin(const Vector3 &origin)
{
    m_origin = origin;

    for (int i = 0; i < num_splits; i++) {
        shadow_renderers[i]->SetOrigin(origin);
    }
}

void PssmShadowMapping::SetVarianceShadowMapping(bool value)
{
    if (value == m_is_variance_shadow_mapping) {
        return;
    }

    for (int i = 0; i < num_splits; i++) {
        shadow_renderers[i]->SetVarianceShadowMapping(value);
    }

    m_is_variance_shadow_mapping = value;
}

void PssmShadowMapping::Render(Renderer *renderer)
{
    // m_depth_shader->SetOverrideCullMode(MaterialFaceCull::MaterialFace_Front);
    // CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_CLAMP);
    for (int i = 0; i < num_splits; i++) {
        shadow_renderers[i]->Begin();

        // TODO: cache beforehand
        Environment::GetInstance()->SetShadowMatrix(i, shadow_renderers[i]->
            GetShadowCamera()->GetViewProjectionMatrix());

        renderer->RenderBucket(
            shadow_renderers[i]->GetShadowCamera(),
            renderer->GetBucket(Renderable::RB_OPAQUE),
            m_depth_shader.get(),
            false
        );

        renderer->RenderBucket(
            shadow_renderers[i]->GetShadowCamera(),
            renderer->GetBucket(Renderable::RB_TRANSPARENT),
            m_depth_shader.get(),
            false
        );

        shadow_renderers[i]->End();
    }
    // CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_CLAMP);
}
} // namespace hyperion
