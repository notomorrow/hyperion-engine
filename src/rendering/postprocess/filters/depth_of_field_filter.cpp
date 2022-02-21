#include "./depth_of_field_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/depth_of_field.h"

namespace hyperion {

DepthOfFieldFilter::DepthOfFieldFilter(float focus_range, float focus_scale)
    : PostFilter(ShaderManager::GetInstance()->GetShader<DepthOfFieldShader>(ShaderProperties())),
      m_focus_range(focus_range),
      m_focus_scale(focus_scale)
{
}

void DepthOfFieldFilter::SetUniforms(Camera *cam)
{
    PostFilter::SetUniforms(cam);

    float blur_coef = 1.0f;
    m_shader->SetUniform("CameraNearFar", Vector2(cam->GetNear(), cam->GetFar()));
    m_shader->SetUniform("Scale", Vector2(1.0f / (cam->GetWidth() * blur_coef), 1.0f / (cam->GetHeight() * blur_coef)));
    m_shader->SetUniform("FocusScale", m_focus_scale);
    m_shader->SetUniform("FocusRange", m_focus_range);
    m_shader->SetUniform("PixelSize", Vector2(1.0f / cam->GetWidth(), 1.0f / cam->GetHeight()));
}

} // namespace hyperion
