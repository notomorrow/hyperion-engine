#include "renderer_sampler.h"
#include "renderer_device.h"
#include "renderer_image_view.h"

#include <system/debug.h>

namespace hyperion {
namespace renderer {
Sampler::Sampler(Texture::TextureFilterMode filter_mode, Texture::TextureWrapMode wrap_mode)
    : m_sampler(nullptr),
      m_filter_mode(filter_mode),
      m_wrap_mode(wrap_mode)
{
}

Sampler::~Sampler()
{
    AssertExitMsg(m_sampler == nullptr, "sampler should have been destroyed");
}

Result Sampler::Create(Device *device, ImageView *image_view)
{
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = ToVkFilter(m_filter_mode);
    sampler_info.minFilter = ToVkFilter(m_filter_mode);

    sampler_info.addressModeU = ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeV = ToVkSamplerAddressMode(m_wrap_mode);
    sampler_info.addressModeW = ToVkSamplerAddressMode(m_wrap_mode);

    sampler_info.anisotropyEnable = true;
    sampler_info.maxAnisotropy = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy;

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = false;

    sampler_info.compareEnable = false;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

    if (m_filter_mode == Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP) {
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    } else {
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }

    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 11.0f;

    if (vkCreateSampler(device->GetDevice(), &sampler_info, nullptr, &m_sampler) != VK_SUCCESS) {
        return Result(Result::RENDERER_ERR, "Failed to create sampler!");
    }

    HYPERION_RETURN_OK;
}

Result Sampler::Destroy(Device *device)
{
    vkDestroySampler(device->GetDevice(), m_sampler, nullptr);

    m_sampler = nullptr;

    HYPERION_RETURN_OK;
}

VkFilter Sampler::ToVkFilter(Texture::TextureFilterMode filter_mode)
{
    switch (filter_mode) {
    case Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST: return VK_FILTER_NEAREST;
    case Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP: // fallthrough
    case Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR: return VK_FILTER_LINEAR;
    }

    unexpected_value_msg(format, "Unhandled texture filter mode case");
}

VkSamplerAddressMode Sampler::ToVkSamplerAddressMode(Texture::TextureWrapMode texture_wrap_mode)
{
    switch (texture_wrap_mode) {
    case Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case Texture::TextureWrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case Texture::TextureWrapMode::TEXTURE_WRAP_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    unexpected_value_msg(format, "Unhandled texture wrap mode case");
}

} // namespace renderer
} // namespace hyperion