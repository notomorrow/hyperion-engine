#include "renderer_image_view.h"
#include "renderer_image.h"
#include "renderer_device.h"

#include <system/debug.h>

namespace hyperion {
namespace renderer {
ImageView::ImageView()
    : m_image_view(nullptr)
{
}

ImageView::ImageView(VkImage image)
    : m_image(image)
{
}

ImageView::~ImageView()
{
    AssertExitMsg(m_image_view == nullptr, "image view should have been destroyed");
}

Result ImageView::Create(
    Device *device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspect_flags,
    VkImageViewType view_type,
    size_t num_mipmaps,
    size_t num_faces)
{

    VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image    = image;
    view_info.viewType = view_type;
    view_info.format   = format;

    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = uint32_t(num_mipmaps);
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = uint32_t(num_faces);

    HYPERION_VK_CHECK_MSG(vkCreateImageView(device->GetDevice(), &view_info, nullptr, &m_image_view), "Failed to create image view");

    HYPERION_RETURN_OK;
}

Result ImageView::Create(Device *device, Image *image)
{
    AssertThrow(image != nullptr);
    AssertThrow(image->GetGPUImage() != nullptr);

    return Create(
        device,
        image->GetGPUImage()->image,
        image->GetImageFormat(),
        ToVkImageAspect(image->GetTextureFormat()),
        ToVkImageViewType(image->GetType()),
        image->NumMipmaps(),
        image->NumFaces()
    );
}

Result ImageView::Destroy(Device *device)
{
    vkDestroyImageView(device->GetDevice(), m_image_view, nullptr);
    m_image_view = nullptr;

    HYPERION_RETURN_OK;
}

VkImageAspectFlags ImageView::ToVkImageAspect(Image::InternalFormat internal_format)
{
    return Image::IsDepthTexture(internal_format)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageViewType ImageView::ToVkImageViewType(Image::Type type)
{
    switch (type) {
    case Image::Type::TEXTURE_TYPE_2D: return VK_IMAGE_VIEW_TYPE_2D;
    case Image::Type::TEXTURE_TYPE_3D: return VK_IMAGE_VIEW_TYPE_3D;
    case Image::Type::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    unexpected_value_msg(format, "Unhandled texture type case");
}

} // namespace renderer
} // namespace hyperion