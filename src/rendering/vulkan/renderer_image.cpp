#include "renderer_image.h"
#include "renderer_pipeline.h"
#include <util/img/image_util.h>
#include <system/debug.h>

#include <vulkan/vulkan.h>

#include <unordered_map>

namespace hyperion {

RendererImage::RendererImage(size_t width, size_t height, size_t depth,
    Texture::TextureInternalFormat format,
    Texture::TextureType type,
    VkImageTiling tiling,
    VkImageUsageFlags image_usage_flags,
    unsigned char *bytes)
    : m_width(width),
      m_height(height),
      m_depth(depth),
      m_format(format),
      m_type(type),
      m_tiling(tiling),
      m_image_usage_flags(image_usage_flags),
      m_image(nullptr),
      m_staging_buffer(nullptr),
      m_bpp(Texture::NumComponents(Texture::GetBaseFormat(format)))
{
    m_size = width * height * depth * m_bpp;

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

RendererResult RendererImage::CreateImage(RendererDevice *device,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info)
{
    VkFormat format = GetImageFormat();
    VkImageType image_type = GetImageType();
    VkImageCreateFlags image_create_flags = 0;
    VkImageFormatProperties image_format_properties{};

    auto format_support_result = device->GetRendererFeatures().GetImageFormatProperties(
        format,
        image_type,
        m_tiling,
        m_image_usage_flags,
        image_create_flags,
        &image_format_properties
    );

    if (!format_support_result) {
        // try a series of fixes to get the image in a valid state.

        std::vector<std::pair<const char *, std::function<RendererResult()>>> potential_fixes;

        // convert to 32bpp image
        if (m_bpp != 4) {
            potential_fixes.emplace_back(std::make_pair(
                "Convert to 32-bpp image",
                [&]() -> RendererResult {
                    return ConvertTo32Bpp(
                        device,
                        image_type,
                        image_create_flags,
                        &image_format_properties,
                        &format
                    );
                }
            ));
        }

        // try a different format
        // TODO

        for (auto &fix : potential_fixes) {
            DebugLog(LogType::Debug, "Attempting fix: '%s' ...\n", fix.first);

            fix.second();

            // try checking format support result again
            if ((format_support_result = device->GetRendererFeatures().GetImageFormatProperties(
                format, image_type, m_tiling, m_image_usage_flags, image_create_flags, &image_format_properties))) {
                DebugLog(LogType::Debug, "Fix '%s' successful!\n", fix.first);
                break;
            }

            DebugLog(LogType::Warn, "Fix '%s' did not change image state to valid.\n", fix.first);
        }

        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = image_type;
    image_info.extent.width = uint32_t(m_width);
    image_info.extent.height = uint32_t(m_height);
    image_info.extent.depth = uint32_t(m_depth);
    image_info.mipLevels = 1; // TODO
    image_info.arrayLayers = (m_type == Texture::TextureType::TEXTURE_TYPE_CUBEMAP) ? 6 : 1;
    image_info.format = format;
    image_info.tiling = m_tiling;
    image_info.initialLayout = initial_layout;//VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = m_image_usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = image_create_flags; // TODO: look into flags for sparse textures for VCT
    *out_image_info = image_info;

    m_image = new RendererGPUImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    HYPERION_BUBBLE_ERRORS(m_image->Create(device, m_size, &image_info));

    HYPERION_RETURN_OK;
}

RendererResult RendererImage::Create(RendererDevice *device, VkImageLayout layout)
{
    VkImageCreateInfo image_info;

    return CreateImage(device, layout, &image_info);
}

RendererResult RendererImage::Create(RendererDevice *device, RendererPipeline *pipeline,
    const LayoutTransferStateBase &transfer_from,
    const LayoutTransferStateBase &transfer_to)
{
    VkImageCreateInfo image_info;
    HYPERION_BUBBLE_ERRORS(CreateImage(device, transfer_from.old_layout, &image_info));

    m_staging_buffer = new RendererStagingBuffer();
    m_staging_buffer->Create(device, m_size);
    m_staging_buffer->Copy(device, m_size, m_bytes);
    // safe to delete m_bytes here?

    auto commands = pipeline->GetSingleTimeCommands();

    { // transition from 'undefined' layout state into one optimal for transfer
        VkImageMemoryBarrier acquire_barrier{},
                             release_barrier{};

        acquire_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        acquire_barrier.oldLayout = transfer_from.old_layout;
        acquire_barrier.newLayout = transfer_from.new_layout;
        acquire_barrier.image = m_image->image;

        commands.Push([this, &image_info, &acquire_barrier, &transfer_from](VkCommandBuffer cmd) {
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            acquire_barrier.subresourceRange = range;
            acquire_barrier.srcAccessMask = transfer_from.src_access_mask;
            acquire_barrier.dstAccessMask = transfer_from.dst_access_mask;

            //barrier the image into the transfer-receive layout
            vkCmdPipelineBarrier(cmd, transfer_from.src_stage_mask, transfer_from.dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &acquire_barrier);
            
            HYPERION_RETURN_OK;
        });

        // copy from staging to image
        commands.Push([this, &image_info, &transfer_from](VkCommandBuffer cmd) {
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
                transfer_from.new_layout,
                1,
                &region
            );

            HYPERION_RETURN_OK;
        });

        // transition from the previous layout state into a shader read-only state
        commands.Push([this, &acquire_barrier, &release_barrier, &transfer_to](VkCommandBuffer cmd) {
            release_barrier = acquire_barrier;

            release_barrier.oldLayout = transfer_to.old_layout;
            release_barrier.newLayout = transfer_to.new_layout;
            release_barrier.srcAccessMask = transfer_to.src_access_mask;
            release_barrier.dstAccessMask = transfer_to.dst_access_mask;

            //barrier the image into the shader readable layout
            vkCmdPipelineBarrier(cmd, transfer_to.src_stage_mask, transfer_to.dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &release_barrier);
            
            HYPERION_RETURN_OK;
        });

        // execute command stack
        HYPERION_BUBBLE_ERRORS(commands.Execute(device));
    }

    // TODO: memory pool to re-use staging buffers

    // destroy staging buffer
    m_staging_buffer->Destroy(device);
    delete m_staging_buffer;
    m_staging_buffer = nullptr;
    
    HYPERION_RETURN_OK;
}

RendererResult RendererImage::Destroy(RendererDevice *device)
{
    {
        auto destroy_image_result = m_image->Destroy(device);
        delete m_image;
        m_image = nullptr;

        if (!destroy_image_result) return destroy_image_result;
    }
    
    HYPERION_RETURN_OK;
}

RendererResult RendererImage::ConvertTo32Bpp(
    RendererDevice *device,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format)
{
    RendererResult format_support_result(RendererResult::RENDERER_OK);

    size_t new_bpp = 4;
    size_t new_size = m_width * m_height * m_depth * new_bpp;

    unsigned char *new_bytes = new unsigned char[new_size];

    ImageUtil::ConvertBpp(m_width, m_height, m_depth, m_bpp, new_bpp, m_bytes, new_bytes);

    delete[] m_bytes;
    m_bytes = new_bytes;

    m_format = Texture::FormatChangeNumComponents(m_format, new_bpp);
    m_bpp = new_bpp;
    m_size = new_size;

    *out_format = GetImageFormat();
    
    HYPERION_RETURN_OK;
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