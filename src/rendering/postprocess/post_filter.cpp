#include "./post_filter.h"

#include "../environment.h"

namespace hyperion {

PostFilter::PostFilter(const std::shared_ptr<PostShader> &shader, BitFlags_t modifies_attachments)
    : m_shader(shader),
      m_modifies_attachments(modifies_attachments)
{
    m_uniform_projection_matrix = m_shader->GetUniforms().Acquire("ProjectionMatrix").id;
    m_uniform_view_matrix = m_shader->GetUniforms().Acquire("ViewMatrix").id;
    m_uniform_resolution = m_shader->GetUniforms().Acquire("Resolution").id;
    m_uniform_cam_near = m_shader->GetUniforms().Acquire("CamNear").id;
    m_uniform_cam_far = m_shader->GetUniforms().Acquire("CamFar").id;
}

void PostFilter::SetUniforms(Camera *cam)
{
    m_shader->SetUniform(m_uniform_view_matrix, cam->GetViewMatrix());
    m_shader->SetUniform(m_uniform_projection_matrix, cam->GetProjectionMatrix());
    m_shader->SetUniform(m_uniform_resolution, Vector2(cam->GetWidth(), cam->GetHeight()));
    m_shader->SetUniform(m_uniform_cam_far, cam->GetFar());
    m_shader->SetUniform(m_uniform_cam_near, cam->GetNear());
}

void PostFilter::Begin(Camera *cam, const Framebuffer::FramebufferAttachments_t &attachments)
{
    for (int i = 0; i < attachments.size(); i++) {
        m_material.SetTexture(
            MaterialTextureKey(Framebuffer::default_texture_attributes[i].material_key),
            attachments[i]
        );
    }

    SetUniforms(cam);

    m_shader->ApplyTransforms(Transform::identity, cam);
    m_shader->ApplyMaterial(m_material);
    m_shader->Use();
}

void PostFilter::End(Camera *cam, Framebuffer *fbo, Framebuffer::FramebufferAttachments_t &attachments, bool copy_textures)
{
    m_shader->End();

    if (!copy_textures) {
        return;
    }

    if (!m_modifies_attachments) {
        return;
    }

    if (fbo == nullptr) {
        return;
    }

    for (int i = 0; i < attachments.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

        if (!ModifiesAttachment(attachment)) {
            continue;
        }

        AssertSoft(attachments[i] != nullptr);
        AssertSoft(Framebuffer::default_texture_attributes[i].is_volatile)

        fbo->Store(attachment, attachments[i]);
    }
}

} // namespace hyperion
