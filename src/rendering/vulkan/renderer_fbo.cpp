#include "renderer_fbo.h"

#include <vulkan/vulkan.h>

namespace hyperion {

RendererFramebufferObject::RendererFramebufferObject(size_t width, size_t height)
    : m_width(width),
      m_height(height),
      m_render_pass(nullptr),
      m_framebuffer{}
{

}

RendererFramebufferObject::~RendererFramebufferObject()
{
    AssertExitMsg(m_render_pass == nullptr, "render pass should have been released");
}

RendererResult RendererFramebufferObject::Create(RendererDevice *device)
{
    m_render_pass = std::make_unique<RendererRenderPass>();

    const auto color_format = device->GetRendererFeatures().FindSupportedFormat(
        std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
    );

    if (color_format == Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_NONE) {
        return RendererResult(
            RendererResult::RENDERER_ERR,
            "No RGBA color format was found with the required features; check debug log for info"
        );
    }

    const auto depth_format = device->GetRendererFeatures().FindSupportedFormat(
        std::array{ Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16,
                    Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );

    if (depth_format == Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_NONE) {
        return RendererResult(
            RendererResult::RENDERER_ERR,
            "No depth format was found; check debug log for info"
        );
    }

    /* Add color attachment */
    m_render_pass->AddAttachment(RendererRenderPass::AttachmentInfo{
        .attachment = std::make_unique<RendererAttachment>(
            helpers::ToVkFormat(color_format),
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
         ),
        .is_depth_attachment = false
    });

    m_fbo_attachments.push_back(AttachmentImageInfo{
        .image = std::make_unique<RendererImage>(
            m_width,
            m_height,
            1,
            color_format,
            Texture::TextureType::TEXTURE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            nullptr
        ),
        .image_view = std::make_unique<RendererImageView>(VK_IMAGE_ASPECT_COLOR_BIT),
        .sampler = std::make_unique<RendererSampler>(
            Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST,
            Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        )
    });

    /* Add depth attachment */
    m_render_pass->AddAttachment(RendererRenderPass::AttachmentInfo{
        .attachment = std::make_unique<RendererAttachment>(
            helpers::ToVkFormat(depth_format),
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
         ),
        .is_depth_attachment = true
    });

    m_fbo_attachments.push_back(AttachmentImageInfo{
        .image = std::make_unique<RendererImage>(
            m_width,
            m_height,
            1,
            depth_format,
            Texture::TextureType::TEXTURE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            nullptr
        ),
        .image_view = std::make_unique<RendererImageView>(VK_IMAGE_ASPECT_DEPTH_BIT),
        .sampler = std::make_unique<RendererSampler>(
            Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST,
            Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        )
    });

    m_render_pass->AddDependency(VkSubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });

    m_render_pass->AddDependency(VkSubpassDependency{
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
    });

    for (auto &image_info : m_fbo_attachments) {
        if (image_info.image != nullptr)
            HYPERION_BUBBLE_ERRORS(image_info.image->Create(device, VK_IMAGE_LAYOUT_UNDEFINED));
        if (image_info.image_view != nullptr)
            HYPERION_BUBBLE_ERRORS(image_info.image_view->Create(device, image_info.image.get()));
        if (image_info.sampler != nullptr)
            HYPERION_BUBBLE_ERRORS(image_info.sampler->Create(device, image_info.image_view.get()));
    }

    HYPERION_BUBBLE_ERRORS(m_render_pass->Create(device));

    std::vector<VkImageView> attachment_image_views;
    { // linear layout of VkImageView data
        attachment_image_views.resize(m_fbo_attachments.size());

        for (size_t i = 0; i < m_fbo_attachments.size(); i++) {
            attachment_image_views[i] = m_fbo_attachments[i].image_view->GetImageView();
        }
    }

    VkFramebufferCreateInfo framebuffer_create_info{};
    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.renderPass = m_render_pass->GetRenderPass();
    framebuffer_create_info.attachmentCount = attachment_image_views.size();
    framebuffer_create_info.pAttachments    = attachment_image_views.data();
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

    for (size_t i = 0; i < m_fbo_attachments.size(); i++) {
        if (m_fbo_attachments[i].sampler != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].sampler->Destroy(device), result);
        if (m_fbo_attachments[i].image_view != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].image_view->Destroy(device), result);
        if (m_fbo_attachments[i].image != nullptr)
            HYPERION_PASS_ERRORS(m_fbo_attachments[i].image->Destroy(device), result);
    }

    m_fbo_attachments.clear();

    HYPERION_PASS_ERRORS(m_render_pass->Destroy(device), result);
    m_render_pass.release();

    return result;
}

} // namespace hyperion
