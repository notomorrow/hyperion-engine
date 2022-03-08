#ifndef HYPERION_RENDERER_IMAGE_VIEW_H
#define HYPERION_RENDERER_IMAGE_VIEW_H

#include "renderer_result.h"
#include "renderer_image.h"
#include "../texture.h"

#include <vulkan/vulkan.h>

namespace hyperion {
class RendererDevice;
class RendererImageView {
public:
    RendererImageView(VkImageAspectFlags aspect_mask);
    RendererImageView(const RendererImageView &other) = delete;
    RendererImageView &operator=(const RendererImageView &other) = delete;
    ~RendererImageView();

    inline VkImageView &GetImageView() { return m_image_view; }
    inline const VkImageView &GetImageView() const { return m_image_view; }

    /* Create imageview independent of a RendererImage */
    RendererResult Create(RendererDevice *device,
        VkImage image,
        VkFormat format,
        VkImageViewType view_type);
    /* Create imageview referencing a RendererImage */
    RendererResult Create(RendererDevice *device,
        RendererImage *image);
    RendererResult Destroy(RendererDevice *device);

private:
    static VkImageViewType ToVkImageViewType(Texture::TextureType);

    VkImageAspectFlags m_aspect_mask;
    VkImageView m_image_view;
};

} // namespace hyperion

#endif