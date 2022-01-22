#include "./post_filter.h"

#include "../environment.h"

namespace apex {

PostFilter::PostFilter(const std::shared_ptr<PostShader> &shader)
{
    m_shader = shader;
}

void PostFilter::Begin(Camera *cam, const Framebuffer::FramebufferAttachments_t &attachments)
{
    for (int i = 0; i < attachments.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = (Framebuffer::FramebufferAttachment)i;

        m_material.SetTexture(
            Framebuffer::default_texture_attributes[attachment].material_key,
            attachments[attachment]
        );
    }

    SetUniforms(cam);

    m_shader->ApplyTransforms(Transform(), cam);
    m_shader->ApplyMaterial(m_material);
    m_shader->Use();
}

void PostFilter::End(Camera *cam, Framebuffer *fbo, Framebuffer::FramebufferAttachments_t &attachments)
{
    m_shader->End();

    for (int i = 0; i < attachments.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = (Framebuffer::FramebufferAttachment)i;

        if (!Framebuffer::default_texture_attributes[attachment].is_volatile) {
            continue;
        }

        if (attachments[attachment] == nullptr) {
            continue;
        }

        fbo->Store(attachment, attachments[attachment]);
    }
}

} // namespace apex
