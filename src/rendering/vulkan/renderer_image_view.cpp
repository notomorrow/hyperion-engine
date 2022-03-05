#include "renderer_image_view.h"
#include "renderer_image.h"
#include "renderer_device.h"

#include <system/debug.h>

namespace hyperion {

RendererImageView::RendererImageView()
    : m_image_view(nullptr)
{
}

RendererImageView::~RendererImageView()
{
    AssertExitMsg(m_image_view == nullptr, "image view should have been destroyed");
}

RendererResult RendererImageView::Create(RendererDevice *device, RendererImage *image)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->GetGPUImage()->image;
    view_info.viewType = ToVkImageViewType(image->GetTextureType());
    view_info.format = image->GetImageFormat();

    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    HYPERION_VK_CHECK_MSG(vkCreateImageView(device->GetDevice(), &view_info, nullptr, &m_image_view), "Failed to create image view");

    return RendererResult(RendererResult::RENDERER_OK);
}

RendererResult RendererImageView::Destroy(RendererDevice *device)
{
    vkDestroyImageView(device->GetDevice(), m_image_view, nullptr);
    m_image_view = nullptr;

    return RendererResult(RendererResult::RENDERER_OK);
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