#include "./ssao_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../environment.h"
#include "../../shaders/post/ssao.h"

namespace hyperion {

SSAOFilter::SSAOFilter(float strength)
    : PostFilter(
        ShaderManager::GetInstance()->GetShader<SSAOShader>(ShaderProperties()
            .Define("CAP_MIN_DISTANCE", 0.00001f)
            .Define("CAP_MAX_DISTANCE", 0.01f)
        ), Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_SSAO),
      m_strength(strength)
{
    m_uniform_strength = m_shader->GetUniforms().Acquire("Strength").id;

    m_shader->SetUniform(m_uniform_strength, m_strength);
}

void SSAOFilter::SetUniforms(Camera *cam)
{
    PostFilter::SetUniforms(cam);
}

} // namespace hyperion
