#ifndef HYPERION_RENDERER_IMAGE_VIEW_H
#define HYPERION_RENDERER_IMAGE_VIEW_H

#include "renderer_result.h"
#include "renderer_image.h"
#include "../texture.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
class Device;
class ImageView {
public:
    ImageView(VkImageAspectFlags aspect_mask);
    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;
    ~ImageView();

    inline VkImageView &GetImageView() { return m_image_view; }
    inline const VkImageView &GetImageView() const { return m_image_view; }

    /* Create imageview independent of a Image */
    Result Create(Device *device,
        VkImage image,
        VkFormat format,
        VkImageViewType view_type,
        size_t num_mipmaps = 1,
        size_t num_faces = 1);
    /* Create imageview referencing a Image */
    Result Create(Device *device,
        Image *image);
    Result Destroy(Device *device);

private:
    static VkImageViewType ToVkImageViewType(Texture::TextureType);

    VkImageAspectFlags m_aspect_mask;
    VkImageView m_image_view;
};

} // namespace renderer
} // namespace hyperion

#endif