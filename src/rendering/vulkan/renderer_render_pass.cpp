#include "renderer_render_pass.h"

namespace hyperion {

RendererRenderPass::RendererRenderPass()
    : m_render_pass{}
{
}

RendererRenderPass::~RendererRenderPass()
{
}

void RendererRenderPass::AddAttachment(AttachmentInfo &&attachment)
{
    AssertExit(attachment.attachment != nullptr);

    m_attachment_infos.push_back(std::move(attachment));
}

RendererResult RendererRenderPass::Create(RendererDevice *device)
{
    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(m_attachment_infos.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    for (size_t i = 0; i < m_attachment_infos.size(); i++) {
        auto &attachment_info = m_attachment_infos[i];

        HYPERION_BUBBLE_ERRORS(attachment_info.attachment->Create(device));

        attachments.push_back(attachment_info.attachment->m_attachment_description);

        if (attachment_info.is_depth_attachment) {
            depth_attachment_ref = attachment_info.attachment->m_attachment_reference;
            subpass_description.pDepthStencilAttachment = &depth_attachment_ref;
        } else {
            color_attachment_refs.push_back(attachment_info.attachment->m_attachment_reference);
        }
    }

    subpass_description.colorAttachmentCount = color_attachment_refs.size();
    subpass_description.pColorAttachments = color_attachment_refs.data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = uint32_t(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = uint32_t(m_dependencies.size());
    render_pass_info.pDependencies = m_dependencies.data();

    HYPERION_VK_CHECK(vkCreateRenderPass(device->GetDevice(), &render_pass_info, nullptr, &m_render_pass));

    HYPERION_RETURN_OK;
}

RendererResult RendererRenderPass::Destroy(RendererDevice *device)
{
    RendererResult result = RendererResult::OK;

    vkDestroyRenderPass(device->GetDevice(), m_render_pass, nullptr);

    return result;
}

void RendererRenderPass::Begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent)
{
    VkRenderPassBeginInfo render_pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = extent;

    const std::array clear_values{
        VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        VkClearValue{.depthStencil = {1.0f, 0}}
    };
    render_pass_info.clearValueCount = clear_values.size();
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void RendererRenderPass::End(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

} // namespace hyperion