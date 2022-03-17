#include "render_pass.h"
#include "../engine.h"

namespace hyperion::v2 {

using renderer::AttachmentBase;

RenderPass::RenderPass(Stage stage, Mode mode)
    : EngineComponent(std::make_unique<renderer::RenderPass>()),
      m_stage(stage),
      m_mode(mode)
{
}

RenderPass::~RenderPass() = default;


void RenderPass::CreateAttachments()
{
    /* Start our binding index at the point where any existing attachments end */
    uint32_t binding_index(m_wrapped->GetColorAttachments().size() + m_wrapped->GetDepthAttachments().size());

    for (auto &it : m_attachments) {
        renderer::RenderPass::AttachmentInfo info{ .is_depth_attachment = renderer::helpers::IsDepthTexture(it.format) };

        switch (m_stage) {
        case RENDER_PASS_STAGE_PRESENT:
            if (info.is_depth_attachment) {
                info.attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>>
                    (binding_index, renderer::helpers::ToVkFormat(it.format));
            } else {
                info.attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR>>
                    (binding_index, renderer::helpers::ToVkFormat(it.format));
            }
            break;
        case RENDER_PASS_STAGE_SHADER:
            if (info.is_depth_attachment) {
                info.attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>>
                    (binding_index, renderer::helpers::ToVkFormat(it.format));
            } else {
                info.attachment = std::make_unique<renderer::Attachment
                    <VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>>
                    (binding_index, renderer::helpers::ToVkFormat(it.format));
            }
            break;
        default:
            AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
        }

        m_wrapped->AddAttachment(std::move(info));

        binding_index++;
    }
}
void RenderPass::CreateDependencies()
{
    switch (m_stage) {
    case RENDER_PASS_STAGE_PRESENT:
        m_wrapped->AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    case RENDER_PASS_STAGE_SHADER:
        m_wrapped->AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        m_wrapped->AddDependency(VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    default:
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

void RenderPass::Create(Engine *engine)
{
    CreateAttachments();
    CreateDependencies();

    auto result = m_wrapped->Create(engine->GetInstance()->GetDevice());
    AssertThrowMsg(result, "%s", result.message);
}

void RenderPass::Destroy(Engine *engine)
{
    auto result = m_wrapped->Destroy(engine->GetInstance()->GetDevice());
    m_wrapped.reset();

    AssertThrowMsg(result, "%s", result.message);
}

} // namespace hyperion::v2