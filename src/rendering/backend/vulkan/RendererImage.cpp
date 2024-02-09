
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <util/img/ImageUtil.hpp>
#include <system/Debug.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

Image<Platform::VULKAN>::Image(
    Extent3D extent,
    InternalFormat format,
    ImageType type,
    FilterMode filter_mode,
    UniquePtr<StreamedData> &&streamed_data,
    ImageFlags flags
) : Image(
        extent,
        format,
        type,
        filter_mode,
        { },
        std::move(streamed_data),
        flags
    )
{
}

Image<Platform::VULKAN>::Image(
    Extent3D extent,
    InternalFormat format,
    ImageType type,
    FilterMode filter_mode,
    const InternalInfo &internal_info,
    UniquePtr<StreamedData> &&streamed_data,
    ImageFlags flags
) : m_extent(extent),
    m_format(format),
    m_type(type),
    m_filter_mode(filter_mode),
    m_internal_info(internal_info),
    m_image(nullptr),
    m_is_blended(false),
    m_is_rw_texture(false),
    m_is_attachment_texture(false),
    m_streamed_data(std::move(streamed_data)),
    m_bpp(NumComponents(GetBaseFormat(format))),
    m_num_layers(1),
    m_flags(flags)
{
    m_size = GetByteSize();
}

Image<Platform::VULKAN>::Image(Image &&other) noexcept
    : m_extent(other.m_extent),
      m_format(other.m_format),
      m_type(other.m_type),
      m_filter_mode(other.m_filter_mode),
      m_internal_info(other.m_internal_info),
      m_is_blended(other.m_is_blended),
      m_is_rw_texture(other.m_is_rw_texture),
      m_is_attachment_texture(other.m_is_attachment_texture),
      m_image(other.m_image),
      m_size(other.m_size),
      m_bpp(other.m_bpp),
      m_streamed_data(std::move(other.m_streamed_data)),
      m_num_layers(other.m_num_layers),
      m_flags(other.m_flags)
{
    other.m_image = nullptr;
    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;
    other.m_extent = Extent3D { };
    other.m_num_layers = 1;
}

Image<Platform::VULKAN> &Image<Platform::VULKAN>::operator=(Image &&other) noexcept
{
    m_extent = other.m_extent;
    m_format = other.m_format;
    m_type = other.m_type;
    m_filter_mode = other.m_filter_mode;
    m_internal_info = other.m_internal_info;
    m_is_blended = other.m_is_blended;
    m_is_rw_texture = other.m_is_rw_texture;
    m_is_attachment_texture = other.m_is_attachment_texture;
    m_image = other.m_image;
    m_size = other.m_size;
    m_bpp = other.m_bpp;
    m_streamed_data = std::move(other.m_streamed_data);
    m_num_layers = other.m_num_layers;
    m_flags = other.m_flags;

    other.m_image = nullptr;
    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;
    other.m_extent = Extent3D { };
    other.m_num_layers = 1;

    return *this;
}

Image<Platform::VULKAN>::~Image()
{
    AssertExit(m_image == nullptr);
}

bool Image<Platform::VULKAN>::IsDepthStencil() const
{
    return IsDepthFormat(m_format);
}

bool Image<Platform::VULKAN>::IsSRGB() const
{
    return IsSRGBFormat(m_format);
}

void Image<Platform::VULKAN>::SetIsSRGB(bool srgb)
{
    const bool is_srgb = IsSRGB();

    if (srgb == is_srgb) {
        return;
    }

    const auto internal_format = m_format;

    if (is_srgb) {
        m_format = InternalFormat(int(internal_format) - int(InternalFormat::SRGB));

        return;
    }

    const auto to_srgb_format = InternalFormat(int(InternalFormat::SRGB) + int(internal_format));

    if (!IsSRGBFormat(to_srgb_format)) {
        DebugLog(
            LogType::Warn,
            "No SRGB counterpart for image type (%d)\n",
            static_cast<int>(internal_format)
        );
    }

    m_format = to_srgb_format;
}

Result Image<Platform::VULKAN>::CreateImage(
    Device<Platform::VULKAN> *device,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info
)
{
    VkFormat format = helpers::ToVkFormat(m_format);
    VkImageType image_type = helpers::ToVkType(m_type);
    VkImageCreateFlags image_create_flags = 0;
    VkFormatFeatureFlags format_features = 0;
    VkImageFormatProperties image_format_properties{};

    m_internal_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    if (m_is_attachment_texture) {
        m_internal_info.usage_flags |= (IsDepthStencil() ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (m_is_rw_texture) {
        m_internal_info.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    } else {
        m_internal_info.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (HasMipmaps()) {
        /* Mipmapped image needs linear blitting. */
        DebugLog(LogType::Debug, "Mipmapped image needs blitting support. Enabling...\n");

        format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        m_internal_info.usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (m_filter_mode) {
        case FilterMode::TEXTURE_FILTER_LINEAR: // fallthrough
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
            format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        }
    }

    if (IsBlended()) {
        DebugLog(LogType::Debug, "Image requires blending, enabling format flag...\n");

        format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (IsTextureCube()) {
        DebugLog(LogType::Debug, "Creating cubemap , enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.\n");

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

        if (!IsDepthFormat(m_format)) {
            // convert to 32bpp image
            if (m_bpp != 4) {
                potential_fixes.emplace_back(std::make_pair(
                    "Convert to 32-bpp image",
                    [&]() -> Result {
                        return ConvertTo32BPP(
                            device,
                            image_type,
                            image_create_flags,
                            &image_format_properties,
                            &format
                        );
                    }
                ));
            }
        }

        for (auto &fix : potential_fixes) {
            DebugLog(LogType::Debug, "Attempting fix: '%s' ...\n", fix.first);

            auto fix_result = fix.second();

            AssertContinueMsg(fix_result, "Image fix function returned an invalid result: %s\n", fix_result.message);

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

    // if (format_features != 0) {
    //     if (!device->GetFeatures().IsSupportedFormat(format, m_internal_info.tiling, format_features)) {
    //         DebugLog(
    //             LogType::Error,
    //             "Device does not support the format %u with requested tiling %d and format features %d\n",
    //             static_cast<uint>(format),
    //             static_cast<int>(m_internal_info.tiling),
    //             static_cast<int>(format_features)
    //         );

    //         HYP_BREAKPOINT;

    //         return Result(Result::RENDERER_ERR, "Format does not support requested features.");
    //     }
    // }

    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32 image_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkImageCreateInfo image_info { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image_info.imageType = image_type;
    image_info.extent.width = m_extent.width;
    image_info.extent.height = m_extent.height;
    image_info.extent.depth = m_extent.depth;
    image_info.mipLevels = NumMipmaps();
    image_info.arrayLayers = NumFaces();
    image_info.format = format;
    image_info.tiling = m_internal_info.tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = m_internal_info.usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = image_create_flags;
    image_info.pQueueFamilyIndices = image_family_indices;
    image_info.queueFamilyIndexCount = uint32(std::size(image_family_indices));

    *out_image_info = image_info;

    m_image = new GPUImageMemory<Platform::VULKAN>();

    HYPERION_BUBBLE_ERRORS(m_image->Create(device, m_size, &image_info));

    HYPERION_RETURN_OK;
}

Result Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    VkImageCreateInfo image_info;

    return CreateImage(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info);
}

Result Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance, ResourceState state)
{
    auto result = Result::OK;

    VkImageCreateInfo image_info;

    HYPERION_BUBBLE_ERRORS(CreateImage(device, VK_IMAGE_LAYOUT_UNDEFINED, &image_info));

    /*VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = NumMipmaps();
    range.baseArrayLayer = 0;
    range.layerCount = NumFaces();*/

    const ImageSubResource sub_resource {
        .num_layers = NumFaces(),
        .num_levels = NumMipmaps()
    };

    auto commands = instance->GetSingleTimeCommands();
    
    StagingBuffer<Platform::VULKAN> staging_buffer;

    if (HasAssignedImageData()) {
        auto loaded_data = m_streamed_data->Load();
        AssertThrow(m_size == loaded_data.Size());

        AssertThrowMsg(m_size % m_bpp == 0, "Invalid image size");
        AssertThrowMsg((m_size / m_bpp) % NumFaces() == 0, "Invalid image size");

        HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

        if (!result) {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;            
        }

        staging_buffer.Copy(device, m_size, loaded_data.Data());

        commands.Push([&](const CommandBufferRef_VULKAN &command_buffer) {
            m_image->InsertBarrier(
                command_buffer,
                sub_resource,
                ResourceState::COPY_DST
            );

            HYPERION_RETURN_OK;
        });

        // copy from staging to image
        const uint num_faces = NumFaces();
        const uint buffer_offset_step = uint(m_size) / num_faces;

        AssertThrowMsg(m_size % buffer_offset_step == 0, "Invalid image size");
        AssertThrowMsg(m_size / buffer_offset_step == num_faces, "Invalid image size");

        for (uint i = 0; i < num_faces; i++) {
            commands.Push([this, &staging_buffer, &image_info, i, buffer_offset_step](const CommandBufferRef_VULKAN &command_buffer)
            {
                volatile uint buffer_size = staging_buffer.size;
                volatile uint buffer_offset_step_ = buffer_offset_step;
                volatile uint total_size = m_size;

                VkBufferImageCopy region { };
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
                    command_buffer->GetCommandBuffer(),
                    staging_buffer.buffer,
                    m_image->image,
                    GPUMemory<Platform::VULKAN>::GetImageLayout(m_image->GetResourceState()),
                    1,
                    &region
                );

                HYPERION_RETURN_OK;
            });
        }

        /* Generate mipmaps if it applies */
        if (HasMipmaps()) {
            /* Assuming device supports this format with linear blitting -- check is done in CreateImage() */

            /* Generate our mipmaps
            */
            commands.Push([this, device](const CommandBufferRef_VULKAN &command_buffer)
            {
                return GenerateMipmaps(device, command_buffer);
            });
        }
    }

    /* Transition from whatever the previous layout state was to our destination state */
    commands.Push([&](const CommandBufferRef_VULKAN &command_buffer) {
        m_image->InsertBarrier(
            command_buffer,
            sub_resource,
            state
        );

        HYPERION_RETURN_OK;
    });

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(device), result);

    if (HasAssignedImageData()) {
        if (result) {
            // destroy staging buffer
            HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
        } else {
            HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
        }

        if (!(m_flags & IMAGE_FLAGS_KEEP_IMAGE_DATA)) {
            m_streamed_data->Unpage();
        }
    }

    return result;
}

Result Image<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    auto result = Result::OK;

    if (m_image != nullptr) {
        HYPERION_PASS_ERRORS(m_image->Destroy(device), result);

        delete m_image;
        m_image = nullptr;
    }

    if (m_streamed_data) {
        m_streamed_data->Unpage();
    }

    return result;
}

Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src
)
{
    return Blit(
        command_buffer,
        src,
        Rect {
            .x0 = 0,
            .y0 = 0,
            .x1 = src->GetExtent().width,
            .y1 = src->GetExtent().height
        },
        Rect {
            .x0 = 0,
            .y0 = 0,
            .x1 = m_extent.width,
            .y1 = m_extent.height
        }
    );
}

Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect src_rect,
    Rect dst_rect
)
{
    const uint num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level = 0
        };

        const ImageSubResource dst {
            .flags = IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level   = 0
        };

        const VkImageAspectFlags aspect_flag_bits = 
            (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        
        /* Blit src -> dst */
        const VkImageBlit blit {
            .srcSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = src.base_mip_level,
                .baseArrayLayer = src.base_array_layer,
                .layerCount     = src.num_layers
            },
            .srcOffsets = {
                { (int32_t) src_rect.x0, (int32_t) src_rect.y0, 0 },
                { (int32_t) src_rect.x1, (int32_t) src_rect.y1, 1 }
            },
            .dstSubresource = {
                .aspectMask = aspect_flag_bits,
                .mipLevel = dst.base_mip_level,
                .baseArrayLayer = dst.base_array_layer,
                .layerCount = dst.num_layers
            },
            .dstOffsets = {
                { (int32_t) dst_rect.x0, (int32_t) dst_rect.y0, 0 },
                { (int32_t) dst_rect.x1, (int32_t) dst_rect.y1, 1 }
            }
        };

        vkCmdBlitImage(
            command_buffer->GetCommandBuffer(),
            src_image->GetGPUImage()->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(src_image->GetGPUImage()->GetResourceState()),
            this->GetGPUImage()->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(this->GetGPUImage()->GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(src_image->GetFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

Result Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect src_rect,
    Rect dst_rect,
    uint src_mip,
    uint dst_mip
)
{
    const auto num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level = src_mip
        };

        const ImageSubResource dst {
            .flags = IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer = face,
            .base_mip_level   = dst_mip
        };

        const VkImageAspectFlags aspect_flag_bits = 
            (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
            | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        
        /* Blit src -> dst */
        const VkImageBlit blit {
            .srcSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = src_mip,
                .baseArrayLayer = src.base_array_layer,
                .layerCount     = src.num_layers
            },
            .srcOffsets = {
                { int32(src_rect.x0), int32(src_rect.y0), 0 },
                { int32(src_rect.x1), int32(src_rect.y1), 1 }
            },
            .dstSubresource = {
                .aspectMask     = aspect_flag_bits,
                .mipLevel       = dst_mip,
                .baseArrayLayer = dst.base_array_layer,
                .layerCount     = dst.num_layers
            },
            .dstOffsets = {
                { int32(dst_rect.x0), int32(dst_rect.y0), 0 },
                { int32(dst_rect.x1), int32(dst_rect.y1), 1 }
            }
        };

        vkCmdBlitImage(
            command_buffer->GetCommandBuffer(),
            src_image->GetGPUImage()->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(src_image->GetGPUImage()->GetResourceState()),
            this->GetGPUImage()->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(this->GetGPUImage()->GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(src_image->GetFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

Result Image<Platform::VULKAN>::GenerateMipmaps(
    Device<Platform::VULKAN> *device,
    CommandBuffer<Platform::VULKAN> *command_buffer
)
{
    if (m_image == nullptr) {
        return { Result::RENDERER_ERR, "Cannot generate mipmaps on uninitialized image" };
    }

    const auto num_faces = NumFaces();
    const auto num_mipmaps = NumMipmaps();

    for (uint32 face = 0; face < num_faces; face++) {
        for (int32 i = 1; i < int32(num_mipmaps + 1); i++) {
            const auto mip_width = int32(helpers::MipmapSize(m_extent.width, i)),
                mip_height = int32(helpers::MipmapSize(m_extent.height, i)),
                mip_depth = int32(helpers::MipmapSize(m_extent.depth, i));

            /* Memory barrier for transfer - note that after generating the mipmaps,
                we'll still need to transfer into a layout primed for reading from shaders. */

            const ImageSubResource src {
                .flags = IsDepthStencil()
                    ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                    : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
                .base_array_layer = face,
                .base_mip_level = uint32(i - 1)
            };

            const ImageSubResource dst {
                .flags = src.flags,
                .base_array_layer = src.base_array_layer,
                .base_mip_level = uint32(i)
            };
            
            m_image->InsertSubResourceBarrier(
                command_buffer,
                src,
                ResourceState::COPY_SRC
            );

            if (i == int32(num_mipmaps)) {
                if (face == num_faces - 1) {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    m_image->SetResourceState(ResourceState::COPY_SRC);
                }

                break;
            }

            const VkImageAspectFlags aspect_flag_bits = 
                (src.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (src.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
                | (dst.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
            /* Blit src -> dst */
            const VkImageBlit blit {
                .srcSubresource = {
                    .aspectMask = aspect_flag_bits,
                    .mipLevel = src.base_mip_level,
                    .baseArrayLayer = src.base_array_layer,
                    .layerCount = src.num_layers
                },
                .srcOffsets = {
                    { 0, 0, 0 },
                    {
                        int32(helpers::MipmapSize(m_extent.width, i - 1)),
                        int32(helpers::MipmapSize(m_extent.height, i - 1)),
                        int32(helpers::MipmapSize(m_extent.depth, i - 1))
                    }
                },
                .dstSubresource = {
                    .aspectMask  = aspect_flag_bits,
                    .mipLevel = dst.base_mip_level,
                    .baseArrayLayer = dst.base_array_layer,
                    .layerCount = dst.num_layers
                },
                .dstOffsets = {
                    { 0, 0, 0 },
                    {
                        mip_width,
                        mip_height,
                        mip_depth
                    }
                }
            };

            vkCmdBlitImage(
                command_buffer->GetCommandBuffer(),
                m_image->image,
                GPUMemory<Platform::VULKAN>::GetImageLayout(ResourceState::COPY_SRC),//m_image->GetSubResourceState(src)),
                m_image->image,
                GPUMemory<Platform::VULKAN>::GetImageLayout(ResourceState::COPY_DST),//m_image->GetSubResourceState(dst)),
                1, &blit,
                IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

void Image<Platform::VULKAN>::CopyFromBuffer(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const GPUBuffer<Platform::VULKAN> *src_buffer
) const
{
    const auto flags = IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const auto num_faces = NumFaces();
    const auto buffer_offset_step = uint(m_size) / num_faces;

    for (uint i = 0; i < num_faces; i++) {
        VkBufferImageCopy region { };
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_extent.width, m_extent.height, m_extent.depth };

        vkCmdCopyBufferToImage(
            command_buffer->GetCommandBuffer(),
            src_buffer->buffer,
            m_image->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(m_image->GetResourceState()),
            1,
            &region
        );
    }
}

void Image<Platform::VULKAN>::CopyToBuffer(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    GPUBuffer<Platform::VULKAN> *dst_buffer
) const
{
    const auto flags = IsDepthStencil()
        ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
        : IMAGE_SUB_RESOURCE_FLAGS_COLOR;

    const VkImageAspectFlags aspect_flag_bits = 
        (flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
            
    // copy from staging to image
    const auto num_faces = NumFaces();
    const auto buffer_offset_step = uint(m_size) / num_faces;

    for (uint i = 0; i < num_faces; i++) {
        VkBufferImageCopy region { };
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_extent.width, m_extent.height, m_extent.depth };

        vkCmdCopyImageToBuffer(
            command_buffer->GetCommandBuffer(),
            m_image->image,
            GPUMemory<Platform::VULKAN>::GetImageLayout(m_image->GetResourceState()),
            dst_buffer->buffer,
            1,
            &region
        );
    }
}

ByteBuffer Image<Platform::VULKAN>::ReadBack(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance) const
{
    StagingBuffer<Platform::VULKAN> staging_buffer;

    auto commands = instance->GetSingleTimeCommands();
    Result result = Result::OK;

    if (HasAssignedImageData()) {
        HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

        if (!result) {
            return ByteBuffer(0, nullptr);
        }


        commands.Push([this, &staging_buffer](const CommandBufferRef_VULKAN &command_buffer) {
            CopyToBuffer(command_buffer, &staging_buffer);

            HYPERION_RETURN_OK;
        });
    }

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(device), result);
    
    if (result) {
        // destroy staging buffer
        HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
    } else {
        HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
    }

    ByteBuffer byte_buffer(m_size);
    staging_buffer.Read(device, m_size, byte_buffer.Data());

    return byte_buffer;
}

Result Image<Platform::VULKAN>::ConvertTo32BPP(
    Device<Platform::VULKAN> *device,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format
)
{
    constexpr uint8 new_bpp = 4;

    const uint num_faces = NumFaces();
    const uint face_offset_step = uint(m_size) / num_faces;

    const uint new_size = num_faces * new_bpp * m_extent.width * m_extent.height * m_extent.depth;
    const uint new_face_offset_step = new_size / num_faces;
    
    if (HasAssignedImageData()) {
        const ByteBuffer byte_buffer = m_streamed_data->Load();
        ByteBuffer new_byte_buffer(new_size);

        for (uint i = 0; i < num_faces; i++) {
            ImageUtil::ConvertBPP(
                m_extent.width, m_extent.height, m_extent.depth,
                m_bpp, new_bpp,
                &byte_buffer.Data()[i * face_offset_step],
                &new_byte_buffer.Data()[i * new_face_offset_step]
            );
        }

        m_streamed_data.Reset(new MemoryStreamedData(std::move(new_byte_buffer)));
    }

    m_format = FormatChangeNumComponents(m_format, new_bpp);
    m_bpp = new_bpp;
    m_size = new_size;

    *out_format = helpers::ToVkFormat(m_format);

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
