#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include "renderer_result.h"
#include "renderer_buffer.h"
#include "../texture.h"

namespace hyperion {
class RendererDevice;
class RendererPipeline;
class RendererImage {
public:
    RendererImage(size_t width, size_t height, size_t depth, Texture::TextureInternalFormat format, Texture::TextureType type, unsigned char *bytes);
    RendererImage(const RendererImage &other) = delete;
    RendererImage &operator=(const RendererImage &other) = delete;
    ~RendererImage();

    RendererResult Create(RendererDevice *device, RendererPipeline *pipeline);
    RendererResult Destroy(RendererDevice *device);

    static VkFormat ToVkFormat(Texture::TextureInternalFormat);
    static VkImageType ToVkType(Texture::TextureType);

private:
    size_t m_width;
    size_t m_height;
    size_t m_depth;
    Texture::TextureInternalFormat m_format;
    Texture::TextureType m_type;
    VkImageTiling m_tiling;
    unsigned char *m_bytes;

    size_t m_size;
    RendererStagingBuffer *m_staging_buffer;
    RendererGPUImage *m_image;
};

} // namespace hyperion

#endif