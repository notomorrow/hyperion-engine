#include "renderer_fbo.h"

#include <vulkan/vulkan.h>

namespace hyperion {

RendererFramebufferObject::RendererFramebufferObject(size_t width, size_t height)
    : m_width(width),
      m_height(height),
      m_render_pass{},
      m_framebuffer{}
{

}

RendererFramebufferObject::~RendererFramebufferObject()
{
}

RendererResult RendererFramebufferObject::Create(RendererDevice *device)
{
    /*m_image = std::make_unique<RendererImage>(
        m_width,
        m_height,
        1,
        Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
        Texture::TextureType::TEXTURE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        nullptr
    );

    HYPERION_BUBBLE_ERRORS(m_image->Create(device, VK_IMAGE_LAYOUT_UNDEFINED));*/

    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(m_attachment_infos.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    for (size_t i = 0; i < m_attachment_infos.size(); i++) {
        auto &attachment_info = m_attachment_infos[i];

        HYPERION_BUBBLE_ERRORS(attachment_info->image.Create(device, VK_IMAGE_LAYOUT_UNDEFINED));
        HYPERION_BUBBLE_ERRORS(attachment_info->image_view.Create(device, &attachment_info->image));
        HYPERION_BUBBLE_ERRORS(attachment_info->sampler.Create(device, &attachment_info->image_view));

        if (attachment_info->image.IsDepthStencilImage()) {
            attachments.push_back(VkAttachmentDescription{
                .format = attachment_info->image.GetImageFormat(),
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            });

            color_attachment_refs.push_back(VkAttachmentReference{
                .attachment = uint32_t(i),
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 
            });
        } else {
            attachments.push_back(VkAttachmentDescription{
                .format = attachment_info->image.GetImageFormat(),
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            });

            depth_attachment_ref = VkAttachmentReference{
                .attachment = uint32_t(i),
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            };

            subpass_description.pDepthStencilAttachment = &depth_attachment_ref;
        }
    }

    subpass_description.colorAttachmentCount = color_attachment_refs.size();
    subpass_description.pColorAttachments = color_attachment_refs.data();

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = uint32_t(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = uint32_t(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    HYPERION_VK_CHECK(vkCreateRenderPass(device->GetDevice(), &render_pass_info, nullptr, &m_render_pass));

    std::vector<VkImageView> attachment_image_views;
    attachment_image_views.resize(m_attachment_infos.size());

    for (size_t i = 0; i < m_attachment_infos.size(); i++) {
        attachment_image_views[i] = m_attachment_infos[i]->image_view.GetImageView();
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = m_render_pass;
    framebuffer_create_info.attachmentCount = attachment_image_views.size();
    framebuffer_create_info.pAttachments = attachment_image_views.data();
    framebuffer_create_info.width = m_width;
    framebuffer_create_info.height = m_height;
    framebuffer_create_info.layers = 1;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_framebuffer));

    HYPERION_RETURN_OK;
}

RendererResult RendererFramebufferObject::Destroy(RendererDevice *device)
{
    RendererResult result = RendererResult::OK;

    vkDestroyFramebuffer(device->GetDevice(), m_framebuffer, nullptr);
    vkDestroyRenderPass(device->GetDevice(), m_render_pass, nullptr);

    for (size_t i = 0; i < m_attachment_infos.size(); i++) {
        HYPERION_PASS_ERRORS(m_attachment_infos[i]->sampler.Destroy(device), result);
        HYPERION_PASS_ERRORS(m_attachment_infos[i]->image_view.Destroy(device), result);
        HYPERION_PASS_ERRORS(m_attachment_infos[i]->image.Destroy(device), result);
    }

    m_attachment_infos.clear();

    return result;
}

void RendererFramebufferObject::AddAttachment(std::unique_ptr<AttachmentInfo> &&attachment)
{
    m_attachment_infos.push_back(std::move(attachment));
}

} // namespace hyperion
