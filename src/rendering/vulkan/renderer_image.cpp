#include "renderer_image.h"
#include "renderer_device.h"
#include "renderer_pipeline.h"
#include <system/debug.h>

#include <vulkan/vulkan.h>

namespace hyperion {

RendererImage::RendererImage(size_t width, size_t height, size_t depth, Texture::TextureInternalFormat format, Texture::TextureType type, unsigned char *bytes)
    : m_width(width),
      m_height(height),
      m_depth(depth),
      m_format(format),
      m_type(type),
      m_tiling(VK_IMAGE_TILING_LINEAR),
      m_image(nullptr),
      m_staging_buffer(nullptr)
{
    m_size = width * height * depth * Texture::NumComponents(Texture::GetBaseFormat(format));

    m_bytes = new unsigned char[m_size];

    if (bytes != nullptr) {
        std::memcpy(m_bytes, bytes, m_size);
    }
}

RendererImage::~RendererImage()
{
    AssertExit(m_image == nullptr);
    AssertExit(m_staging_buffer == nullptr);

    delete[] m_bytes;
}

RendererResult RendererImage::Create(RendererDevice *device, RendererPipeline *pipeline)
{
    VkFormat format = ToVkFormat(m_format); // TODO: pick best available image type

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = ToVkType(m_type);
    image_info.extent.width = uint32_t(m_width);
    image_info.extent.height = uint32_t(m_height);
    image_info.extent.depth = uint32_t(m_depth);
    image_info.mipLevels = 1; // TODO:
    image_info.arrayLayers = (m_type == Texture::TextureType::TEXTURE_TYPE_CUBEMAP) ? 6 : 1;
    image_info.format = format;
    image_info.tiling = m_tiling;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = 0; // TODO: look into flags for sparse textures for VCT


    m_staging_buffer = new RendererStagingBuffer();
    m_staging_buffer->Create(device, m_size);
    m_staging_buffer->Copy(device, m_size, m_bytes);
    // safe to delete m_bytes here?

    m_image = new RendererGPUImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    m_image->Create(device, m_size, &image_info);

    auto commands = pipeline->GetSingleTimeCommands();

    { // transition from 'undefined' layout state into one optimal for transfer
        auto transition_layout_result = commands.TransitionImageLayout(
            device,
            m_image->image,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        if (!transition_layout_result) return transition_layout_result;
    }

    {
        // copy from staging to image
        auto copy_staging_buffer_result = commands.Execute(device, [this, &image_info](VkCommandBuffer cmd) {
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = image_info.extent;

            vkCmdCopyBufferToImage(
                cmd,
                m_staging_buffer->buffer,
                m_image->image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
            );  

            return RendererResult(RendererResult::RENDERER_OK);
        });

        if (!copy_staging_buffer_result) return copy_staging_buffer_result;
    }

    { // transition from the previous layout state into a shader read-only state
        auto transition_layout_result = commands.TransitionImageLayout(
            device,
            m_image->image,
            format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        if (!transition_layout_result) return transition_layout_result;
    }


    // destroy staging buffer
    m_staging_buffer->Destroy(device);
    delete m_staging_buffer;
    m_staging_buffer = nullptr;

}

RendererResult RendererImage::Destroy(RendererDevice *device)
{
    {
        auto destroy_image_result = m_image->Destroy(device);
        delete m_image;
        m_image = nullptr;

        if (!destroy_image_result) return destroy_image_result;
    }

    return RendererResult(RendererResult::RENDERER_OK);
}

VkFormat RendererImage::ToVkFormat(Texture::TextureInternalFormat fmt)
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

VkImageType RendererImage::ToVkType(Texture::TextureType type)
{
    switch (type) {
    case Texture::TextureType::TEXTURE_TYPE_2D: return VK_IMAGE_TYPE_2D;
    case Texture::TextureType::TEXTURE_TYPE_3D: return VK_IMAGE_TYPE_3D;
    case Texture::TextureType::TEXTURE_TYPE_CUBEMAP: return VK_IMAGE_TYPE_2D;
    }

    unexpected_value_msg(format, "Unhandled texture type case");
}

} // namespace hyperion