#ifndef HYPERION_RENDERER_IMAGE_VIEW_H
#define HYPERION_RENDERER_IMAGE_VIEW_H

#include "renderer_result.h"
#include "../texture.h"

#include <vulkan/vulkan.h>

namespace hyperion {
class RendererImage;
class RendererDevice;
class RendererImageView {
public:
    RendererImageView();
    RendererImageView(const RendererImageView &other) = delete;
    RendererImageView &operator=(const RendererImageView &other) = delete;
    ~RendererImageView();

    RendererResult Create(RendererDevice *device, RendererImage *image);
    RendererResult Destroy(RendererDevice *device);

private:
    static VkImageViewType ToVkImageViewType(Texture::TextureType);

    VkImage m_image;
    VkImageView m_image_view;
};

} // namespace hyperion

#endif