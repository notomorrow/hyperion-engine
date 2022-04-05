#include "renderer_render_pass.h"

#include "renderer_command_buffer.h"

namespace hyperion {
namespace renderer {

RenderPassAttachmentRef::~RenderPassAttachmentRef()
{
    AssertThrowMsg(
        !m_is_created,
        "Expected render pass attachment reference to not be in `created` state on destructor call"
    );
}

RenderPassAttachmentRef::RenderPassAttachmentRef(
    RenderPassAttachment *attachment,
    LoadOperation load_operation,
    StoreOperation store_operation
) : RenderPassAttachmentRef(
        attachment,
        std::make_unique<ImageView>(),
        std::make_unique<Sampler>(),
        load_operation,
        store_operation
    )
{
}

RenderPassAttachmentRef::RenderPassAttachmentRef(
    RenderPassAttachment *attachment,
    std::unique_ptr<ImageView> &&image_view,
    std::unique_ptr<Sampler> &&sampler,
    LoadOperation load_operation,
    StoreOperation store_operation
) : m_attachment(attachment),
    m_load_operation(load_operation),
    m_store_operation(store_operation),
    m_binding{},
    m_initial_layout(attachment->GetInitialLayout()),
    m_final_layout(attachment->GetFinalLayout()),
    m_image_view(std::move(image_view)),
    m_sampler(std::move(sampler))
{
}

VkAttachmentLoadOp RenderPassAttachmentRef::ToVkLoadOp(LoadOperation load_operation)
{
    switch (load_operation) {
    case LoadOperation::UNDEFINED:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::NONE:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    case LoadOperation::CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    default:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp RenderPassAttachmentRef::ToVkStoreOp(StoreOperation store_operation)
{
    switch (store_operation) {
    case StoreOperation::UNDEFINED:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::NONE:
        return VK_ATTACHMENT_STORE_OP_NONE_EXT;
    case StoreOperation::STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

VkImageLayout RenderPassAttachmentRef::GetIntermediateLayout() const
{
    return m_attachment->IsDepthAttachment()
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

Image::InternalFormat RenderPassAttachmentRef::GetFormat() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_NONE;
    }

    return m_attachment->GetFormat();
}

bool RenderPassAttachmentRef::IsDepthAttachment() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return false;
    }

    return m_attachment->IsDepthAttachment();
}

VkAttachmentDescription RenderPassAttachmentRef::GetAttachmentDescription() const
{
    return VkAttachmentDescription{
        .format         = Image::ToVkFormat(m_attachment->GetFormat()),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = ToVkLoadOp(m_load_operation),
        .storeOp        = ToVkStoreOp(m_store_operation),
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        /* TODO: Image should hold initial and final layouts */
        .initialLayout  = m_initial_layout,
        .finalLayout    = m_final_layout
    };
}

VkAttachmentReference RenderPassAttachmentRef::GetAttachmentReference() const
{
    if (!HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetAttachmentReference() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    return VkAttachmentReference{
        .attachment = GetBinding(),
        .layout     = GetIntermediateLayout()
    };
}

void RenderPassAttachmentRef::IncRef() const
{
    AssertThrow(m_ref_count != nullptr);

    ++m_ref_count->count;
}

void RenderPassAttachmentRef::DecRef() const
{
    AssertThrow(m_ref_count != nullptr);
    AssertThrow(m_ref_count->count != 0);

    --m_ref_count->count;
}

Result RenderPassAttachmentRef::Create(Device *device)
{
    AssertThrow(!m_is_created);

    HYPERION_BUBBLE_ERRORS(m_image_view->Create(device, m_attachment->GetImage()));
    HYPERION_BUBBLE_ERRORS(m_sampler->Create(device, m_image_view.get()));

    m_is_created = true;

    HYPERION_RETURN_OK;
}


Result RenderPassAttachmentRef::Create(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    size_t num_mipmaps,
    size_t num_faces)
{
    AssertThrow(!m_is_created);
    
    HYPERION_BUBBLE_ERRORS(m_image_view->Create(
        device,
        image,
        format,
        aspect_flags,
        view_type,
        num_mipmaps,
        num_faces
    ));

    HYPERION_BUBBLE_ERRORS(m_sampler->Create(device, m_image_view.get()));

    m_is_created = true;

    HYPERION_RETURN_OK;
}

Result RenderPassAttachmentRef::Destroy(Device *device)
{
    AssertThrow(m_is_created);

    HYPERION_BUBBLE_ERRORS(m_image_view->Destroy(device));
    HYPERION_BUBBLE_ERRORS(m_sampler->Destroy(device));

    m_is_created = false;

    HYPERION_RETURN_OK;
}

Result RenderPassAttachmentRef::AddAttachmentRef(Device *device, StoreOperation store_operation, RenderPassAttachmentRef **out)
{
    auto result = Result::OK;

    HYPERION_BUBBLE_ERRORS(result = m_attachment->AddAttachmentRef(device, LoadOperation::LOAD, store_operation, out));

    (*out)->m_initial_layout = m_final_layout;
    (*out)->m_final_layout   = m_final_layout;

    return result;
}

RenderPass::RenderPass(Stage stage, Mode mode)
    : m_stage(stage),
      m_mode(mode),
      m_render_pass{}
{
}

RenderPass::~RenderPass()
{
    for (const auto *attachment_ref : m_render_pass_attachment_refs) {
        attachment_ref->DecRef();
    }

    AssertThrowMsg(m_render_pass == nullptr, "render pass should have been destroyed");
}

void RenderPass::CreateDependencies()
{
    switch (m_stage) {
    case Stage::RENDER_PASS_STAGE_PRESENT:
        AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        });

        break;
    case Stage::RENDER_PASS_STAGE_SHADER:
        AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        });

        AddDependency(VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        });
        
        AddDependency(VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        });
        
        AddDependency(VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        });

        break;
    default:
        AssertThrowMsg(0, "Unsupported stage type %d", m_stage);
    }
}

Result RenderPass::Create(Device *device)
{
    CreateDependencies();

    std::vector<VkAttachmentDescription> attachments;
    attachments.reserve(m_render_pass_attachment_refs.size());

    VkAttachmentReference depth_attachment_ref{};
    std::vector<VkAttachmentReference> color_attachment_refs;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.pDepthStencilAttachment = nullptr;

    /* NEW */
    uint32_t next_binding = 0;

    for (auto *attachment_ref : m_render_pass_attachment_refs) {
        if (!attachment_ref->HasBinding()) {
            attachment_ref->SetBinding(next_binding);
        }

        next_binding = attachment_ref->GetBinding() + 1;

        attachments.push_back(attachment_ref->GetAttachmentDescription());

        if (attachment_ref->IsDepthAttachment()) {
            depth_attachment_ref = attachment_ref->GetAttachmentReference();
            subpass_description.pDepthStencilAttachment = &depth_attachment_ref;

            m_clear_values.push_back(VkClearValue{.depthStencil = {1.0f, 0}});
        } else {
            color_attachment_refs.push_back(attachment_ref->GetAttachmentReference());

            m_clear_values.push_back(VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}});
        }
    }

    subpass_description.colorAttachmentCount = uint32_t(color_attachment_refs.size());
    subpass_description.pColorAttachments    = color_attachment_refs.data();

    // Create the actual renderpass
    VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = uint32_t(attachments.size());
    render_pass_info.pAttachments    = attachments.data();
    render_pass_info.subpassCount    = 1;
    render_pass_info.pSubpasses      = &subpass_description;
    render_pass_info.dependencyCount = uint32_t(m_dependencies.size());
    render_pass_info.pDependencies   = m_dependencies.data();

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

void RenderPass::Begin(CommandBuffer *cmd, VkFramebuffer framebuffer, VkExtent2D extent)
{
    VkRenderPassBeginInfo render_pass_info{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = extent;
    render_pass_info.clearValueCount = uint32_t(m_clear_values.size());
    render_pass_info.pClearValues = m_clear_values.data();

    VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE;

    switch (m_mode) {
    case Mode::RENDER_PASS_INLINE:
        contents = VK_SUBPASS_CONTENTS_INLINE;
        break;
    case Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER:
        contents = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
        break;
    }

    vkCmdBeginRenderPass(cmd->GetCommandBuffer(), &render_pass_info, contents);
}

void RenderPass::End(CommandBuffer *cmd)
{
    vkCmdEndRenderPass(cmd->GetCommandBuffer());
}

} // namespace renderer
} // namespace hyperion