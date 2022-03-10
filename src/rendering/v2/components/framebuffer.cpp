#include "framebuffer.h"

namespace hyperion::v2 {

using renderer::Attachment;

Framebuffer::Framebuffer(size_t width, size_t height)
    : BaseComponent(std::make_unique<renderer::FramebufferObject>(width, height))
{
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::Create(Instance *instance, RenderPass *render_pass)
{
    /*const auto &attachment_infos = m_wrapped->GetAttachmentImageInfos();

    std::vector<renderer::RenderPass::AttachmentInfo> color_attachments,
                                                      depth_attachments;

    color_attachments.reserve(attachment_infos.size() - 1);
    depth_attachments.reserve(1);

    for (size_t i = 0; i < attachment_infos.size(); i++) {
        auto format = Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;

        AssertThrowMsg(attachment_infos[i].image != nullptr, "Cannot create default RenderPass when now Image object is set!");

        if (attachment_infos[i].image != nullptr) {
            format = attachment_infos[i].image->GetTextureFormat();
        }

        if (renderer::helpers::IsDepthTexture(format)) {
            depth_attachments.push_back(renderer::RenderPass::AttachmentInfo{
                .attachment = std::make_unique<Attachment>(
                    renderer::helpers::ToVkFormat(format),
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    i,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = true
            });
        } else {
            color_attachments.push_back(renderer::RenderPass::AttachmentInfo{
                .attachment = std::make_unique<Attachment>(
                    renderer::helpers::ToVkFormat(format),
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    i,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                ),
                .is_depth_attachment = false
            });
        }
    }

    for (size_t i = render_pass->GetWrappedObject()->GetColorAttachments().size(); i < color_attachments.size(); i++) {
        render_pass->GetWrappedObject()->AddAttachment(std::move(color_attachments[i]));
    }

    for (size_t i = render_pass->GetWrappedObject()->GetDepthAttachments().size(); i < depth_attachments.size(); i++) {
        render_pass->GetWrappedObject()->AddAttachment(std::move(depth_attachments[i]));
    }*/

    auto result = m_wrapped->Create(instance->GetDevice(), render_pass->GetWrappedObject());
    AssertThrowMsg(result, "%s", result);
}

void Framebuffer::Destroy(Instance *instance)
{
    auto result = m_wrapped->Destroy(instance->GetDevice());
    AssertThrowMsg(result, "%s", result);
}

} // namespace hyperion::v2