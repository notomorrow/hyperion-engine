#include "renderer_fbo.h"
#include "renderer_render_pass.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
FramebufferObject::FramebufferObject(uint32_t width, uint32_t height)
    : m_width(width),
      m_height(height),
      m_framebuffer(nullptr)
{
}

FramebufferObject::~FramebufferObject()
{
    AssertExitMsg(m_framebuffer == nullptr, "framebuffer should have been destroyed");
}

Result FramebufferObject::Create(Device *device, RenderPass *render_pass)
{
    AssertThrowMsg(!m_render_pass_attachment_refs.empty(), "At least one attachment must be added");
    
    std::vector<VkImageView> attachment_image_views;
    attachment_image_views.reserve(m_render_pass_attachment_refs.size());
    
    for (auto *attachment_ref : m_render_pass_attachment_refs) {
        AssertThrow(attachment_ref != nullptr);
        AssertThrow(attachment_ref->GetImageView() != nullptr);
        AssertThrow(attachment_ref->GetImageView()->GetImageView() != nullptr);

        attachment_image_views.push_back(attachment_ref->GetImageView()->GetImageView());
    }

    VkFramebufferCreateInfo framebuffer_create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer_create_info.renderPass      = render_pass->GetRenderPass();
    framebuffer_create_info.attachmentCount = uint32_t(attachment_image_views.size());
    framebuffer_create_info.pAttachments    = attachment_image_views.data();
    framebuffer_create_info.width           = m_width;
    framebuffer_create_info.height          = m_height;
    framebuffer_create_info.layers          = 1;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_framebuffer));

    HYPERION_RETURN_OK;
}

Result FramebufferObject::Destroy(Device *device)
{
    Result result = Result::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_framebuffer, nullptr);
    m_framebuffer = nullptr;
    
    for (auto *attachment_ref : m_render_pass_attachment_refs) {
        attachment_ref->DecRef();
    }

    m_render_pass_attachment_refs.clear();

    return result;
}

} // namespace renderer
} // namespace hyperion
