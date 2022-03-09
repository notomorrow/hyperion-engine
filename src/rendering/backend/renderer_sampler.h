#ifndef RENDERER_SAMPLER_H
#define RENDERER_SAMPLER_H

#include "renderer_result.h"
#include "../texture.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
class ImageView;
class Device;
class Sampler {
public:
    Sampler(Texture::TextureFilterMode filter_mode, Texture::TextureWrapMode wrap_mode);
    Sampler(const Sampler &other) = delete;
    Sampler &operator=(const Sampler &other) = delete;
    ~Sampler();

    inline VkSampler &GetSampler() { return m_sampler; }
    inline const VkSampler &GetSampler() const { return m_sampler; }

    Result Create(Device *device, ImageView *image_view);
    Result Destroy(Device *device);

private:
    static VkFilter ToVkFilter(Texture::TextureFilterMode);
    static VkSamplerAddressMode ToVkSamplerAddressMode(Texture::TextureWrapMode);

    VkSampler m_sampler;
    Texture::TextureFilterMode m_filter_mode;
    Texture::TextureWrapMode m_wrap_mode;
};

} // namespace renderer
} // namespace hyperion

#endif