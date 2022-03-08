#include "renderer_image_view.h"
#include "renderer_image.h"
#include "renderer_device.h"

#include <system/debug.h>

namespace hyperion {

RendererImageView::RendererImageView(VkImageAspectFlags aspect_mask)
    : m_aspect_mask(aspect_mask),
      m_image_view(nullptr)
{
}

RendererImageView::~RendererImageView()
{
    AssertExitMsg(m_image_view == nullptr, "image view should have been destroyed");
}

RendererResult RendererImageView::Create(RendererDevice *device,
    VkImage image,
    VkFormat format,
    VkImageViewType view_type,
    size_t num_mipmaps)
{

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = view_type;
    view_info.format = format;

    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    view_info.subresourceRange.aspectMask = m_aspect_mask;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = num_mipmaps;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    HYPERION_VK_CHECK_MSG(vkCreateImageView(device->GetDevice(), &view_info, nullptr, &m_image_view), "Failed to create image view");

    HYPERION_RETURN_OK;
}

RendererResult RendererImageView::Create(RendererDevice *device,
    RendererImage *image)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        image->GetImageFormat(),
        ToVkImageViewType(image->GetTextureType()),
        image->GetNumMipmaps()
    );
}

RendererResult RendererImageView::Destroy(RendererDevice *device)
{
    vkDestroyImageView(device->GetDevice(), m_image_view, nullptr);
    m_image_view = nullptr;

    HYPERION_RETURN_OK;
}

VkImageViewType RendererImageView::ToVkImageViewType(Texture::TextureType type)
{
    switch (type) {
    case Texture::TextureType::TEXTURE_TYPE_2D: return VK_IMAGE_VIEW_TYPE_2D;
    case Texture::TextureType::TEXTURE_TYPE_3D: return VK_IMAGE_VIEW_TYPE_3D;
    case Texture::TextureType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    unexpected_value_msg(format, "Unhandled texture type case");
}

} // namespace hyperion