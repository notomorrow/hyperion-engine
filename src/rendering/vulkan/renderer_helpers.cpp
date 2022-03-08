#include "renderer_helpers.h"
#include <math/math_util.h>

namespace hyperion {
namespace helpers {


bool IsDepthTexture(Texture::TextureInternalFormat fmt)
{
    return IsDepthTexture(Texture::GetBaseFormat(fmt));
}

bool IsDepthTexture(Texture::TextureBaseFormat fmt)
{
    return fmt == Texture::TextureBaseFormat::TEXTURE_FORMAT_DEPTH;
}

size_t MipmapSize(size_t src_size, int lod)
{
    return MathUtil::Max(src_size >> lod, 1ull);
}

VkFormat ToVkFormat(Texture::TextureInternalFormat fmt)
{
    switch (fmt) {
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R8: return VK_FORMAT_R8_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG8: return VK_FORMAT_R8G8_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8: return VK_FORMAT_R8G8B8_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8: return VK_FORMAT_R8G8B8A8_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R16: return VK_FORMAT_R16_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG16: return VK_FORMAT_R16G16_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16: return VK_FORMAT_R16G16B16_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R16F: return VK_FORMAT_R16_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F: return VK_FORMAT_R16G16_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16F: return VK_FORMAT_R16G16B16_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R32F: return VK_FORMAT_R32_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F: return VK_FORMAT_R32G32_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F: return VK_FORMAT_R32G32B32_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16: return VK_FORMAT_D16_UNORM;
        //case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24: return VK_FORMAT_D24_UNORM_S8_UINT;
        //case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32: return VK_FORMAT_D32_UNORM;
    case Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F: return VK_FORMAT_D32_SFLOAT;
    }

    unexpected_value_msg(format, "Unhandled texture format case");
}

VkImageType ToVkType(Texture::TextureType type)
{
    switch (type) {
    case Texture::TextureType::TEXTURE_TYPE_2D: return VK_IMAGE_TYPE_2D;
    case Texture::TextureType::TEXTURE_TYPE_3D: return VK_IMAGE_TYPE_3D;
    case Texture::TextureType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_TYPE_2D;
    }

    unexpected_value_msg(format, "Unhandled texture type case");
}

} // namespace helpers
} // namespace hyperion