#include "./post_filter.h"

#include "../environment.h"

namespace apex {

PostFilter::PostFilter(const std::shared_ptr<PostShader> &shader)
{
    m_shader = shader;
}

void PostFilter::Begin(Camera *cam, Framebuffer *fbo)
{
    m_material.texture0 = fbo->GetColorTexture();
    m_material.texture1 = fbo->GetDepthTexture();
    m_material.texture2 = fbo->GetPositionTexture();
    m_material.normals0 = fbo->GetNormalTexture();

    //m_material.texture0 = Environment::GetInstance()->GetShadowMap(3);

    SetUniforms(cam);

    m_shader->ApplyMaterial(m_material);
    m_shader->Use();
}

void PostFilter::End(Camera *cam, Framebuffer *fbo)
{
    m_shader->End();

    fbo->StoreColor();
    fbo->StoreDepth();
}

} // namespace apex
