#include "renderer_fbo.h"
#include "renderer_render_pass.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
FramebufferObject::FramebufferObject(size_t width, size_t height)
    : m_width(width),
      m_height(height),
      m_framebuffer(nullptr)
{
}

FramebufferObject::~FramebufferObject()
{
    AssertExitMsg(m_framebuffer == nullptr, "framebuffer should have been destroyed");
}

Result FramebufferObject::AddAttachment(Texture::TextureInternalFormat format)
{
    return AddAttachment(AttachmentImageInfo{
        .image = std::make_unique<FramebufferImage2D>(m_width, m_height, format, nullptr),
        .image_view = nullptr,
        .sampler = nullptr,
        .image_needs_creation = true,
        .image_view_needs_creation = true,
        .sampler_needs_creation = true
    }, format);
}

Result FramebufferObject::AddAttachment(AttachmentImageInfo &&image_info, Texture::TextureInternalFormat format)
{
    VkImageAspectFlags image_aspect_flags = (helpers::IsDepthTexture(format))
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;

    if (image_info.image_view == nullptr) {
        image_info.image_view = std::make_unique<ImageView>(image_aspect_flags);

        image_info.image_view_needs_creation = true;
    }

    if (image_info.sampler == nullptr) {
        image_info.sampler = std::make_unique<Sampler>(
            Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST,
            Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        );

        image_info.sampler_needs_creation = true;
    }

    m_fbo_attachments.push_back(std::move(image_info));

    HYPERION_RETURN_OK;
}

Result FramebufferObject::Create(Device *device, RenderPass *render_pass)
{
    AssertThrowMsg(!m_fbo_attachments.empty(), "At least one attachment must be added");

    for (auto &image_info : m_fbo_attachments) {
        if (image_info.image != nullptr && image_info.image_needs_creation) {

            /* Create the image in gpu memory -- no texture data is copied */
            HYPERION_BUBBLE_ERRORS(image_info.image->Create(device));
        }
        if (image_info.image_view != nullptr && image_info.image_view_needs_creation) {
            AssertThrowMsg(image_info.image != nullptr, "If image_view is to be created, image needs to be valid.");
            HYPERION_BUBBLE_ERRORS(image_info.image_view->Create(device, image_info.image.get()));
        }
        if (image_info.sampler != nullptr && image_info.sampler_needs_creation) {
            AssertThrowMsg(image_info.image_view != nullptr, "If sampler is to be created, image_view needs to be valid.");
            HYPERION_BUBBLE_ERRORS(image_info.sampler->Create(device, image_info.image_view.get()));
        }
    }

    std::vector<VkImageView> attachment_image_views;
    { // linear layout of VkImageView data
        attachment_image_views.resize(m_fbo_attachments.size());

        for (size_t i = 0; i < m_fbo_attachments.size(); i++) {
            attachment_image_views[i] = m_fbo_attachments[i].image_view->GetImageView();
        }
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = render_pass->GetRenderPass();
    framebuffer_create_info.attachmentCount = uint32_t(attachment_image_views.size());
    framebuffer_create_info.pAttachments    = attachment_image_views.data();
    framebuffer_create_info.width = uint32_t(m_width);
    framebuffer_create_info.height = uint32_t(m_height);
    framebuffer_create_info.layers = 1;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_framebuffer));

    HYPERION_RETURN_OK;
}

Result FramebufferObject::Destroy(Device *device)
{
    Result result = Result::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_framebuffer, nullptr);
    m_framebuffer = nullptr;

    for (size_t i = 0; i < m_fbo_attachments.size(); i++) {
        if (m_fbo_attachments[i].sampler != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].sampler->Destroy(device), result);
        if (m_fbo_attachments[i].image_view != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].image_view->Destroy(device), result);
        if (m_fbo_attachments[i].image != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].image->Destroy(device), result);
    }

    m_fbo_attachments.clear();

    return result;
}

} // namespace renderer
} // namespace hyperion
