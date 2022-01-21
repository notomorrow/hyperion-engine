#include "./post_filter.h"

#include "../environment.h"

namespace apex {

PostFilter::PostFilter(const std::shared_ptr<PostShader> &shader)
{
    m_shader = shader;
}

void PostFilter::Begin(Camera *cam, Framebuffer2D *fbo)
{
    // TODO: initialization
    m_material.SetTexture("ColorMap", fbo->GetColorTexture());
    m_material.SetTexture("DepthMap", fbo->GetDepthTexture());
    m_material.SetTexture("PositionMap", fbo->GetPositionTexture());
    m_material.SetTexture("NormalMap", fbo->GetNormalTexture());
    m_material.SetTexture("DataMap", fbo->GetDataTexture());
    m_material.SetTexture("SSLightingMap", fbo->GetAoTexture());

    SetUniforms(cam);

    m_shader->ApplyTransforms(Transform(), cam);
    m_shader->ApplyMaterial(m_material);
    m_shader->Use();
}

void PostFilter::End(Camera *cam, Framebuffer2D *fbo)
{
    m_shader->End();

    // fbo->Store(fbo->GetColorTexture(), 0);

    // // TODO: check
    // fbo->Store(fbo->GetAoTexture(), 4);
}

} // namespace apex
