#include "renderer_render_pass.h"

namespace hyperion {
namespace renderer {
RenderPass::RenderPass()
    : m_render_pass{}
{
}

RenderPass::~RenderPass()
{
    AssertExitMsg(m_render_pass == nullptr, "render pass should have been destroyed");
}

void RenderPass::AddAttachment(AttachmentInfo &&attachment)
{
    AssertExit(attachment.attachment != nullptr);

    if (attachment.is_depth_attachment) {
        AssertThrowMsg(m_depth_attachments.empty(), "May only have one depth attachment in a renderpass!");

        m_depth_attachments.push_back(std::move(attachment));
    } else {
        m_color_attachments.push_back(std::move(attachment));
    }
}

Result RenderPass::Create(Device *device)
{
    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(m_color_attachments.size() + m_depth_attachments.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    for (size_t i = 0; i < m_color_attachments.size(); i++) {
        auto &attachment_info = m_color_attachments[i];

        HYPERION_BUBBLE_ERRORS(attachment_info.attachment->Create(device));

        attachments.push_back(attachment_info.attachment->m_attachment_description);
        color_attachment_refs.push_back(attachment_info.attachment->m_attachment_reference);
    }

    /* Should only loop one time due to assertion in AddAttachment.
     * Still, keeping it consistent. */
    for (size_t i = 0; i < m_depth_attachments.size(); i++) {
        auto &attachment_info = m_depth_attachments[i];

        HYPERION_BUBBLE_ERRORS(attachment_info.attachment->Create(device));

        attachments.push_back(attachment_info.attachment->m_attachment_description);

        depth_attachment_ref = attachment_info.attachment->m_attachment_reference;
        subpass_description.pDepthStencilAttachment = &depth_attachment_ref;
    }

    subpass_description.colorAttachmentCount = color_attachment_refs.size();
    subpass_description.pColorAttachments    = color_attachment_refs.data();

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

Result RenderPass::Destroy(Device *device)
{
    Result result = Result::OK;

    vkDestroyRenderPass(device->GetDevice(), m_render_pass, nullptr);
    m_render_pass = nullptr;

    return result;
}

void RenderPass::Begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D extent, VkSubpassContents contents)
{
    VkRenderPassBeginInfo render_pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = { 0, 0 };
    render_pass_info.renderArea.extent = extent;

    // TODO: pre-generate this data

    std::vector<VkClearValue> clear_values;
    clear_values.resize(m_color_attachments.size() + m_depth_attachments.size());

    for (int i = 0; i < m_color_attachments.size(); i++) {
        clear_values[i] = VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}};
    }

    for (int i = 0; i < m_depth_attachments.size(); i++) {
        clear_values[m_color_attachments.size() + i] = VkClearValue{.depthStencil = {1.0f, 0}};
    }

    render_pass_info.clearValueCount = clear_values.size();
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd, &render_pass_info, contents);
}

void RenderPass::End(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

} // namespace renderer
} // namespace hyperion