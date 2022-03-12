#include "renderer_image.h"
#include "renderer_pipeline.h"
#include "renderer_instance.h"
#include <util/img/image_util.h>
#include <system/debug.h>

#include <vulkan/vulkan.h>

#include <unordered_map>

namespace hyperion {
namespace renderer {
Image::Image(size_t width, size_t height, size_t depth,
    Texture::TextureInternalFormat format,
    Texture::TextureType type,
    Texture::TextureFilterMode filter_mode,
    const InternalInfo &internal_info,
    unsigned char *bytes)
    : m_width(width),
      m_height(height),
      m_depth(depth),
      m_format(format),
      m_type(type),
      m_filter_mode(filter_mode),
      m_internal_info(internal_info),
      m_image(nullptr),
      m_staging_buffer(nullptr),
      m_bpp(Texture::NumComponents(Texture::GetBaseFormat(format)))
{
    m_size = width * height * depth * m_bpp * GetNumFaces();

    m_bytes = new unsigned char[m_size];

    if (bytes != nullptr) {
        std::memcpy(m_bytes, bytes, m_size);
    }
}

Image::~Image()
{
    AssertExit(m_image == nullptr);
    AssertExit(m_staging_buffer == nullptr);

    delete[] m_bytes;
}

Result Image::CreateImage(Device *device,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info)
{
    VkFormat format = GetImageFormat();
    VkImageType image_type = GetImageType();
    VkImageCreateFlags image_create_flags = 0;
    VkFormatFeatureFlags format_features = 0;
    VkImageFormatProperties image_format_properties{};

    if (IsMipmappedImage()) {
        /* Mipmapped image needs linear blitting. */
        DebugLog(LogType::Debug, "Mipmapped image needs linear blitting support. Checking...\n");

        format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

        m_internal_info.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (IsCubemap()) {


        DebugLog(LogType::Debug, "Creating cubemap texture, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.\n");

        image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    auto format_support_result = device->GetFeatures().GetImageFormatProperties(
        format,
        image_type,
        m_internal_info.tiling,
        m_internal_info.usage_flags,
        image_create_flags,
        &image_format_properties
    );

    if (!format_support_result) {
        // try a series of fixes to get the image in a valid state.

        std::vector<std::pair<const char *, std::function<Result()>>> potential_fixes;

        // convert to 32bpp image
        if (m_bpp != 4) {
            potential_fixes.emplace_back(std::make_pair(
                "Convert to 32-bpp image",
                [&]() -> Result {
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
            if ((format_support_result = device->GetFeatures().GetImageFormatProperties(
                format, image_type, m_internal_info.tiling, m_internal_info.usage_flags, image_create_flags, &image_format_properties))) {
                DebugLog(LogType::Debug, "Fix '%s' successful!\n", fix.first);
                break;
            }

            DebugLog(LogType::Warn, "Fix '%s' did not change image state to valid.\n", fix.first);
        }

        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    if (format_features != 0) {
        if (!device->GetFeatures().IsSupportedFormat(format, m_internal_info.tiling, format_features)) {
            return Result(Result::RENDERER_ERR, "Format does not support requested features.");
        }
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = image_type;
    image_info.extent.width = uint32_t(m_width);
    image_info.extent.height = uint32_t(m_height);
    image_info.extent.depth = uint32_t(m_depth);
    image_info.mipLevels = GetNumMipmaps();
    image_info.arrayLayers = GetNumFaces();
    image_info.format = format;
    image_info.tiling = m_internal_info.tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = m_internal_info.usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = image_create_flags; // TODO: look into flags for sparse textures for VCT
    *out_image_info = image_info;

    m_image = new GPUImage(m_internal_info.usage_flags);

    HYPERION_BUBBLE_ERRORS(m_image->Create(device, m_size, &image_info));

    HYPERION_RETURN_OK;
}

Result Image::Create(Device *device, VkImageLayout layout)
{
    VkImageCreateInfo image_info;

    return CreateImage(device, layout, &image_info);
}

Result Image::Create(Device *device, Instance *renderer,
    LayoutTransferStateBase transfer_from,
    LayoutTransferStateBase transfer_to)
{
    VkImageCreateInfo image_info;
    HYPERION_BUBBLE_ERRORS(CreateImage(device, transfer_from.src.layout, &image_info));

    m_staging_buffer = new StagingBuffer();
    m_staging_buffer->Create(device, m_size);
    m_staging_buffer->Copy(device, m_size, m_bytes);
    // safe to delete m_bytes here?

    auto commands = renderer->GetSingleTimeCommands();

    { // transition from 'undefined' layout state into one optimal for transfer
        VkImageMemoryBarrier acquire_barrier{},
                             release_barrier{};

        acquire_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        acquire_barrier.oldLayout = transfer_from.src.layout;
        acquire_barrier.newLayout = transfer_from.dst.layout;
        acquire_barrier.image = m_image->image;

        commands.Push([this, &image_info, &acquire_barrier, &transfer_from](VkCommandBuffer cmd) {
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = GetNumMipmaps(); /* all mip levels will be in `transfer_from.new_layout` */
            range.baseArrayLayer = 0;
            range.layerCount = GetNumFaces();

            acquire_barrier.subresourceRange = range;
            acquire_barrier.srcAccessMask = transfer_from.src.access_mask;
            acquire_barrier.dstAccessMask = transfer_from.dst.access_mask;

            // barrier the image into the transfer-receive layout
            vkCmdPipelineBarrier(
                cmd,
                transfer_from.src.stage_mask,
                transfer_from.dst.stage_mask,
                0,
                0, nullptr,
                0, nullptr,
                1,
                &acquire_barrier
            );

            HYPERION_RETURN_OK;
        });

        // copy from staging to image
        int num_faces = GetNumFaces();
        size_t buffer_offset_step = m_size / num_faces;

        for (int i = 0; i < num_faces; i++) {

            commands.Push([this, &image_info, &transfer_from, i, buffer_offset_step](VkCommandBuffer cmd) {
                VkBufferImageCopy region{};
                region.bufferOffset = i * buffer_offset_step;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = 0;
                region.imageSubresource.baseArrayLayer = i;
                region.imageSubresource.layerCount = 1;
                region.imageOffset = { 0, 0, 0 };
                region.imageExtent = image_info.extent;

                vkCmdCopyBufferToImage(
                    cmd,
                    m_staging_buffer->buffer,
                    m_image->image,
                    transfer_from.dst.layout,
                    1,
                    &region
                );

                HYPERION_RETURN_OK;
            });
        }

        /* Generate mipmaps if it applies */
        if (IsMipmappedImage()) {
            AssertThrowMsg(m_bytes != nullptr, "Cannot generate mipmaps on an image with no bytes set");

            /* Assuming device supports this format with linear blitting -- check is done in CreateImage() */

            /* Generate our mipmaps. We'll need to be doing it here as we need
               the layout to be `transfer_from.new_layout`
            */
            commands.Push([this, &transfer_from, &transfer_to](VkCommandBuffer cmd) {
                LayoutTransferStateBase intermediate{
                    .src = transfer_from.dst, /* Use the output from the previous command as an input */
                    .dst = {
                        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .access_mask = VK_ACCESS_TRANSFER_READ_BIT,
                        .stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT
                    }
                };

                const size_t num_mipmaps = GetNumMipmaps();
                const size_t num_faces = GetNumFaces();

                for (int i = 1; i < num_mipmaps + 1; i++) {
                    int32_t mip_width = helpers::MipmapSize(m_width, i),
                            mip_height = helpers::MipmapSize(m_height, i),
                            mip_depth = helpers::MipmapSize(m_depth, i);

                    /* Memory barrier for transfer - note that after generating the mipmaps,
                        we'll still need to transfer into a layout primed for reading from shaders. */
                    VkImageMemoryBarrier mipmap_barrier{};
                    mipmap_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    mipmap_barrier.image = m_image->image;
                    mipmap_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    mipmap_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    mipmap_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    mipmap_barrier.subresourceRange.baseArrayLayer = 0;
                    mipmap_barrier.subresourceRange.layerCount = 1;
                    mipmap_barrier.subresourceRange.levelCount = 1;
                    mipmap_barrier.oldLayout = intermediate.src.layout;
                    mipmap_barrier.newLayout = intermediate.dst.layout;
                    mipmap_barrier.srcAccessMask = intermediate.src.access_mask;
                    mipmap_barrier.dstAccessMask = intermediate.dst.access_mask;
                    mipmap_barrier.subresourceRange.baseMipLevel = i - 1;

                    /* Transfer the prev mipmap into a format for reading */
                    vkCmdPipelineBarrier(
                        cmd,
                        intermediate.src.stage_mask,
                        intermediate.dst.stage_mask,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &mipmap_barrier
                    );

                    if (i == num_mipmaps) {
                        /*
                         * Use the output from this intermediate step as an input
                         * into the next stage
                         */
                        transfer_to.src = intermediate.dst;

                        break;
                    }

                    /* We swap src and dst as we will use the newly transfered image as an input to our mipmap level */
                    std::swap(intermediate.dst, intermediate.src);

                    /* Blit the image into the subresource */
                    VkImageBlit blit{};
                    blit.srcOffsets[0] = { 0, 0, 0 };
                    blit.srcOffsets[1] = {
                        int32_t(helpers::MipmapSize(m_width, i - 1)),
                        int32_t(helpers::MipmapSize(m_height, i - 1)),
                        int32_t(helpers::MipmapSize(m_depth, i - 1))
                    };
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel = i - 1; /* Read from previous mip level */
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount = 1;
                    blit.dstOffsets[0] = { 0, 0, 0 };
                    blit.dstOffsets[1] = {
                        mip_width,
                        mip_height,
                        mip_depth
                    };
                    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel = i;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount = 1;

                    vkCmdBlitImage(cmd,
                        m_image->image,
                        intermediate.src.layout,
                        m_image->image,
                        intermediate.dst.layout,
                        1, &blit,
                        VK_FILTER_LINEAR
                    );

                    /* We swap again, as we are using this as an input into the next iteration */
                    std::swap(intermediate.src, intermediate.dst);
                }

                HYPERION_RETURN_OK;
            });
        }

        // transition from the previous layout state into a shader read-only state
        commands.Push([this, &acquire_barrier, &release_barrier, &transfer_to](VkCommandBuffer cmd) {
            release_barrier = acquire_barrier;

            release_barrier.oldLayout = transfer_to.src.layout;
            release_barrier.newLayout = transfer_to.dst.layout;
            release_barrier.srcAccessMask = transfer_to.src.access_mask;
            release_barrier.dstAccessMask = transfer_to.dst.access_mask;

            //barrier the image into the shader readable layout
            vkCmdPipelineBarrier(
                cmd,
                transfer_to.src.stage_mask,
                transfer_to.dst.stage_mask,
                0,
                0, nullptr,
                0, nullptr,
                1,
                &release_barrier
            );

            HYPERION_RETURN_OK;
        });

        // execute command stack
        HYPERION_BUBBLE_ERRORS(commands.Execute(device));
    }

    // destroy staging buffer
    m_staging_buffer->Destroy(device);
    delete m_staging_buffer;
    m_staging_buffer = nullptr;

    HYPERION_RETURN_OK;
}

Result Image::Destroy(Device *device)
{
    Result result = Result::OK;

    HYPERION_PASS_ERRORS(m_image->Destroy(device), result);

    delete m_image;
    m_image = nullptr;

    return result;
}

Result Image::ConvertTo32Bpp(
    Device *device,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format)
{
    const size_t num_faces = GetNumFaces();
    const size_t face_offset_step = m_size / num_faces;

    const size_t new_bpp = 4;
    const size_t new_size = m_width * m_height * m_depth * new_bpp * num_faces;
    const size_t new_face_offset_step = new_size / num_faces;

    unsigned char *new_bytes = new unsigned char[new_size];

    for (int i = 0; i < num_faces; i++) {
        ImageUtil::ConvertBpp(
            m_width, m_height, m_depth,
            m_bpp, new_bpp,
            &m_bytes[i * face_offset_step],
            &new_bytes[i * new_face_offset_step]
        );
    }

    delete[] m_bytes;
    m_bytes = new_bytes;

    m_format = Texture::FormatChangeNumComponents(m_format, new_bpp);
    m_bpp = new_bpp;
    m_size = new_size;

    *out_format = GetImageFormat();

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion