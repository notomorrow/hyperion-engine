/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererHelpers.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

VkIndexType ToVkIndexType(DatumType datum_type)
{
    switch (datum_type) {
    case DatumType::UNSIGNED_BYTE:
        return VK_INDEX_TYPE_UINT8_EXT;
    case DatumType::UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case DatumType::UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        AssertThrowMsg(0, "Unsupported datum type to vulkan index type conversion: %d", int(datum_type));
    }
}

VkFormat ToVkFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::R8:          return VK_FORMAT_R8_UNORM;
    case InternalFormat::RG8:         return VK_FORMAT_R8G8_UNORM;
    case InternalFormat::RGB8:        return VK_FORMAT_R8G8B8_UNORM;
    case InternalFormat::RGBA8:       return VK_FORMAT_R8G8B8A8_UNORM;
    case InternalFormat::R8_SRGB:     return VK_FORMAT_R8_SRGB;
    case InternalFormat::RG8_SRGB:    return VK_FORMAT_R8G8_SRGB;
    case InternalFormat::RGB8_SRGB:   return VK_FORMAT_R8G8B8_SRGB;
    case InternalFormat::RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
    case InternalFormat::R32_:        return VK_FORMAT_R32_UINT;
    case InternalFormat::RG16_:       return VK_FORMAT_R16G16_UNORM;
    case InternalFormat::R11G11B10F:  return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case InternalFormat::R10G10B10A2: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case InternalFormat::R16:         return VK_FORMAT_R16_UNORM;
    case InternalFormat::RG16:        return VK_FORMAT_R16G16_UNORM;
    case InternalFormat::RGB16:       return VK_FORMAT_R16G16B16_UNORM;
    case InternalFormat::RGBA16:      return VK_FORMAT_R16G16B16A16_UNORM;
    case InternalFormat::R32:         return VK_FORMAT_R32_UINT;
    case InternalFormat::RG32:        return VK_FORMAT_R32G32_UINT;
    case InternalFormat::RGB32:       return VK_FORMAT_R32G32B32_UINT;
    case InternalFormat::RGBA32:      return VK_FORMAT_R32G32B32A32_UINT;
    case InternalFormat::R16F:        return VK_FORMAT_R16_SFLOAT;
    case InternalFormat::RG16F:       return VK_FORMAT_R16G16_SFLOAT;
    case InternalFormat::RGB16F:      return VK_FORMAT_R16G16B16_SFLOAT;
    case InternalFormat::RGBA16F:     return VK_FORMAT_R16G16B16A16_SFLOAT;
    case InternalFormat::R32F:        return VK_FORMAT_R32_SFLOAT;
    case InternalFormat::RG32F:       return VK_FORMAT_R32G32_SFLOAT;
    case InternalFormat::RGB32F:      return VK_FORMAT_R32G32B32_SFLOAT;
    case InternalFormat::RGBA32F:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        
    case InternalFormat::BGRA8:       return VK_FORMAT_B8G8R8A8_UNORM;
    case InternalFormat::BGR8_SRGB:   return VK_FORMAT_B8G8R8_SRGB;
    case InternalFormat::BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
    case InternalFormat::DEPTH_16:    return VK_FORMAT_D16_UNORM_S8_UINT;
    case InternalFormat::DEPTH_24:    return VK_FORMAT_D24_UNORM_S8_UINT;
    case InternalFormat::DEPTH_32F:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
    default: break;
    }

    AssertThrowMsg(false, "Unhandled texture format case %d", int(fmt));
}

VkImageType ToVkType(ImageType type)
{
    switch (type) {
    case ImageType::TEXTURE_TYPE_2D: return VK_IMAGE_TYPE_2D;
    case ImageType::TEXTURE_TYPE_3D: return VK_IMAGE_TYPE_3D;
    case ImageType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_TYPE_2D;
    }

    AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
}

VkFilter ToVkFilter(FilterMode filter_mode)
{
    switch (filter_mode) {
    case FilterMode::TEXTURE_FILTER_NEAREST: // fallthrough
    case FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP:
        return VK_FILTER_NEAREST;
    case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP: // fallthrough
    case FilterMode::TEXTURE_FILTER_LINEAR:
        return VK_FILTER_LINEAR;
    default:
        break;
    }

    AssertThrowMsg(false, "Unhandled texture filter mode case %d", int(filter_mode));
}

VkSamplerAddressMode ToVkSamplerAddressMode(WrapMode texture_wrap_mode)
{
    switch (texture_wrap_mode) {
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case WrapMode::TEXTURE_WRAP_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkImageAspectFlags ToVkImageAspect(InternalFormat internal_format)
{
    return IsDepthFormat(internal_format)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageViewType ToVkImageViewType(ImageType type, bool is_array)
{
    if (is_array) {
        switch (type) {
        case ImageType::TEXTURE_TYPE_2D:      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case ImageType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        }

        AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
    }

    switch (type) {
    case ImageType::TEXTURE_TYPE_2D:      return VK_IMAGE_VIEW_TYPE_2D;
    case ImageType::TEXTURE_TYPE_3D:      return VK_IMAGE_VIEW_TYPE_3D;
    case ImageType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_VIEW_TYPE_CUBE;
    }

    AssertThrowMsg(false, "Unhandled texture type case %d", int(type));
}

#pragma region SingleTimeCommands

Result SingleTimeCommands::Begin(Device *device)
{
    m_fence = MakeRenderObject<platform::Fence<Platform::VULKAN>>();

    command_buffer = MakeRenderObject<platform::CommandBuffer<Platform::VULKAN>>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
    command_buffer->GetPlatformImpl().command_pool = pool;
    HYPERION_BUBBLE_ERRORS(command_buffer->Create(device));

    return command_buffer->Begin(device);
}

Result SingleTimeCommands::End(Device *device)
{
    Result result;

    HYPERION_PASS_ERRORS(command_buffer->End(device), result);

    HYPERION_PASS_ERRORS(m_fence->Create(device), result);
    HYPERION_PASS_ERRORS(m_fence->Reset(device), result);

    // Submit to the queue
    platform::DeviceQueue<Platform::VULKAN> &queue_graphics = device->GetGraphicsQueue();

    HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(&queue_graphics, m_fence, nullptr), result);
    
    HYPERION_PASS_ERRORS(m_fence->WaitForGPU(device), result);
    HYPERION_PASS_ERRORS(m_fence->Destroy(device), result);

    HYPERION_PASS_ERRORS(command_buffer->Destroy(device), result);

    return result;
}

#pragma endregion SingleTimeCommands

} // namespace renderer
} // namespace helpers
} // namespace hyperion