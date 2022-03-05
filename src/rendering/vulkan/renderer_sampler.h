#ifndef RENDERER_SAMPLER_H
#define RENDERER_SAMPLER_H

#include "renderer_result.h"
#include "../texture.h"

#include <vulkan/vulkan.h>

namespace hyperion {

class RendererImageView;
class RendererDevice;
class RendererSampler {
public:
    RendererSampler(Texture::TextureFilterMode filter_mode, Texture::TextureWrapMode wrap_mode);
    RendererSampler(const RendererSampler &other) = delete;
    RendererSampler &operator=(const RendererSampler &other) = delete;
    ~RendererSampler();

    inline VkSampler &GetSampler() { return m_sampler; }
    inline const VkSampler &GetSampler() const { return m_sampler; }

    RendererResult Create(RendererDevice *device, RendererImageView *image_view);
    RendererResult Destroy(RendererDevice *device);

private:
    static VkFilter ToVkFilter(Texture::TextureFilterMode);
    static VkSamplerAddressMode ToVkSamplerAddressMode(Texture::TextureWrapMode);

    VkSampler m_sampler;
    Texture::TextureFilterMode m_filter_mode;
    Texture::TextureWrapMode m_wrap_mode;
};

} // namespace hyperion

#endif