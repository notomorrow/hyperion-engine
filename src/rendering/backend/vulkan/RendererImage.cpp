/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <util/img/ImageUtil.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Pair.hpp>

#include <core/functional/Proc.hpp>

#include <core/debug/Debug.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

extern VkImageLayout GetVkImageLayout(ResourceState);
extern VkAccessFlags GetVkAccessMask(ResourceState);
extern VkPipelineStageFlags GetVkShaderStageMask(ResourceState, bool, ShaderModuleType);

#pragma region ImagePlatformImpl

RendererResult ImagePlatformImpl<Platform::VULKAN>::ConvertTo32BPP(
    Device<Platform::VULKAN> *device,
    const TextureData *in_texture_data,
    VkImageType image_type,
    VkImageCreateFlags image_create_flags,
    VkImageFormatProperties *out_image_format_properties,
    VkFormat *out_format,
    UniquePtr<TextureData> &out_texture_data
)
{
    constexpr uint8 new_bpp = 4;

    const uint32 num_faces = self->NumFaces();

    const TextureDesc &current_desc = self->GetTextureDesc();

    const uint32 size = self->m_size;
    const uint32 face_offset_step = size / num_faces;

    const uint8 bpp = self->m_bpp;

    const uint32 new_size = num_faces * new_bpp * current_desc.extent.x * current_desc.extent.y * current_desc.extent.z;
    const uint32 new_face_offset_step = new_size / num_faces;
    
    const InternalFormat new_format = FormatChangeNumComponents(current_desc.format, new_bpp);

    if (in_texture_data != nullptr) {
        AssertThrow(in_texture_data->buffer.Size() == size);

        ByteBuffer new_byte_buffer(new_size);

        for (uint32 i = 0; i < num_faces; i++) {
            ImageUtil::ConvertBPP(
                current_desc.extent.x, current_desc.extent.y, current_desc.extent.z,
                bpp, new_bpp,
                &in_texture_data->buffer.Data()[i * face_offset_step],
                &new_byte_buffer.Data()[i * new_face_offset_step]
            );
        }

        self->m_texture_desc = TextureDesc {
            current_desc.type,
            new_format,
            current_desc.extent,
            current_desc.filter_mode_min,
            current_desc.filter_mode_mag,
            current_desc.wrap_mode,
            current_desc.num_layers
        };

        out_texture_data.Emplace(TextureData {
            self->m_texture_desc,
            std::move(new_byte_buffer)
        });
    }

    self->m_bpp = new_bpp;
    self->m_size = new_size;

    *out_format = helpers::ToVkFormat(self->m_texture_desc.format);

    HYPERION_RETURN_OK;
}

RendererResult ImagePlatformImpl<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    const TextureData *in_texture_data,
    VkImageLayout initial_layout,
    VkImageCreateInfo *out_image_info,
    UniquePtr<TextureData> &out_texture_data
)
{
    if (!is_handle_owned) {
        AssertThrowMsg(handle != VK_NULL_HANDLE, "If is_handle_owned is set to false, the image handle must not be VK_NULL_HANDLE.");

        HYPERION_RETURN_OK;
    }

    const Vec3u extent = self->GetExtent();
    const InternalFormat format = self->GetTextureFormat();
    const ImageType type = self->GetType();

    const bool is_attachment_texture = self->IsAttachmentTexture();
    const bool is_rw_texture = self->IsRWTexture();

    const bool is_depth_stencil = self->IsDepthStencil();
    const bool is_blended = self->IsBlended();
    const bool is_srgb = self->IsSRGB();

    const bool is_cubemap = self->IsTextureCube();

    const bool has_mipmaps = self->HasMipmaps();
    const uint32 num_mipmaps = self->NumMipmaps();

    const uint32 num_faces = self->NumFaces();

    if (extent.Volume() == 0) {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    VkFormat vk_format = helpers::ToVkFormat(format);
    VkImageType vk_image_type = helpers::ToVkType(type);
    VkImageCreateFlags vk_image_create_flags = 0;
    VkFormatFeatureFlags vk_format_features = 0;
    VkImageFormatProperties vk_image_format_properties { };

    tiling = VK_IMAGE_TILING_OPTIMAL;

    if (is_attachment_texture) {
        usage_flags |= (is_depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (is_rw_texture) {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    } else {
        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    if (has_mipmaps) {
        /* Mipmapped image needs linear blitting. */
        vk_format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (self->GetMinFilterMode()) {
        case FilterMode::TEXTURE_FILTER_LINEAR: // fallthrough
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (is_blended) {
        HYP_LOG(RenderingBackend, Debug, "Image requires blending, enabling format flag...");

        vk_format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (is_cubemap) {
        HYP_LOG(RenderingBackend, Debug, "Creating cubemap, enabling VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag.");

        vk_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    auto format_support_result = device->GetFeatures().GetImageFormatProperties(
        vk_format,
        vk_image_type,
        tiling,
        usage_flags,
        vk_image_create_flags,
        &vk_image_format_properties
    );

    if (!format_support_result) {
        // try a series of fixes to get the image in a valid state.

        Array<Pair<const char *, Proc<RendererResult()>>> potential_fixes;

        if (!IsDepthFormat(self->GetTextureFormat())) {
            // convert to 32bpp image
            if (self->GetBPP() != 4) {
                potential_fixes.EmplaceBack(
                    "Convert to 32-bpp image",
                    [&]() -> RendererResult
                    {
                        return ConvertTo32BPP(
                            device,
                            in_texture_data,
                            vk_image_type,
                            vk_image_create_flags,
                            &vk_image_format_properties,
                            &vk_format,
                            out_texture_data
                        );
                    }
                );
            }
        }

        for (auto &fix : potential_fixes) {
            HYP_LOG(RenderingBackend, Debug, "Attempting fix '{}'...\n", fix.first);

            RendererResult fix_result = fix.second();

            if (!fix_result) {
                HYP_LOG(RenderingBackend, Debug, "Fix '{}' failed: {}", fix.first, fix_result.GetError().GetMessage());

                continue;
            }

            // try checking format support result again
            if ((format_support_result = device->GetFeatures().GetImageFormatProperties(
                vk_format, vk_image_type, tiling, usage_flags, vk_image_create_flags, &vk_image_format_properties))) {
                
                HYP_LOG(RenderingBackend, Debug, "Fix '{}' succeeded.\n", fix.first);

                break;
            }

            HYP_LOG(RenderingBackend, Debug, "Fix '{}' failed: {}", fix.first, format_support_result.GetError().GetMessage());
        }

        HYPERION_BUBBLE_ERRORS(format_support_result);
    }

    // if (format_features != 0) {
    //     if (!device->GetFeatures().IsSupportedFormat(format, m_internal_info.tiling, format_features)) {
    //         DebugLog(
    //             LogType::Error,
    //             "Device does not support the format %u with requested tiling %d and format features %d\n",
    //             static_cast<uint32>(format),
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
    image_info.imageType = vk_image_type;
    image_info.extent.width = extent.x;
    image_info.extent.height = extent.y;
    image_info.extent.depth = extent.z;
    image_info.mipLevels = num_mipmaps;
    image_info.arrayLayers = num_faces;
    image_info.format = vk_format;
    image_info.tiling = tiling;
    image_info.initialLayout = initial_layout;
    image_info.usage = usage_flags;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = vk_image_create_flags;
    image_info.pQueueFamilyIndices = image_family_indices;
    image_info.queueFamilyIndexCount = uint32(std::size(image_family_indices));

    *out_image_info = image_info;

    VmaAllocationCreateInfo alloc_info { };
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    HYPERION_VK_CHECK_MSG(
        vmaCreateImage(
            device->GetAllocator(),
            &image_info,
            &alloc_info,
            &handle,
            &allocation,
            nullptr
        ),
        "Failed to create gpu image!"
    );

    HYPERION_RETURN_OK;
}

RendererResult ImagePlatformImpl<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (allocation != VK_NULL_HANDLE) {
        AssertThrowMsg(is_handle_owned, "If allocation is not VK_NULL_HANDLE, is_handle_owned should be true");

        vmaDestroyImage(device->GetAllocator(), handle, allocation);
        allocation = VK_NULL_HANDLE;
    }

    handle = VK_NULL_HANDLE;

    // reset back to default
    is_handle_owned = true;

    HYPERION_RETURN_OK;
}

void ImagePlatformImpl<Platform::VULKAN>::SetResourceState(ResourceState new_state)
{
    resource_state = new_state;

    sub_resources.Clear();
}

void ImagePlatformImpl<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state,
    ImageSubResourceFlagBits flags
)
{
    InsertBarrier(
        command_buffer,
        ImageSubResource {
            .flags = flags,
            .num_layers = VK_REMAINING_ARRAY_LAYERS,
            .num_levels = VK_REMAINING_MIP_LEVELS
        },
        new_state
    );
}

void ImagePlatformImpl<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    /* Clear any sub-resources that are in a separate state */
    if (!sub_resources.Empty()) {
        sub_resources.Clear();
    }

    if (handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to insert a resource barrier but image was not defined"
        );

        return;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range { };
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = GetVkImageLayout(resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = handle;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(resource_state, true, ShaderModuleType::UNSET),
        GetVkShaderStageMask(new_state, false, ShaderModuleType::UNSET),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    resource_state = new_state;
}

void ImagePlatformImpl<Platform::VULKAN>::InsertSubResourceBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    if (handle == VK_NULL_HANDLE) {
        HYP_LOG(
            RenderingBackend,
            Debug,
            "Attempt to insert a resource barrier but image was not defined"
        );

        return;
    }

    ResourceState prev_resource_state = resource_state;

    auto it = sub_resources.Find(sub_resource);

    if (it != sub_resources.End()) {
        prev_resource_state = it->second;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range { };
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = GetVkImageLayout(prev_resource_state);
    barrier.newLayout = GetVkImageLayout(new_state);
    barrier.srcAccessMask = GetVkAccessMask(prev_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.image = handle;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = range;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(prev_resource_state, true, ShaderModuleType::UNSET),
        GetVkShaderStageMask(new_state, false, ShaderModuleType::UNSET),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    sub_resources.Insert({ sub_resource, new_state });
}

auto ImagePlatformImpl<Platform::VULKAN>::GetSubResourceState(const ImageSubResource &sub_resource) const -> ResourceState
{
    const auto it = sub_resources.Find(sub_resource);

    if (it == sub_resources.End()) {
        return resource_state;
    }

    return it->second;
}

void ImagePlatformImpl<Platform::VULKAN>::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
{
    sub_resources[sub_resource] = new_state;
}

#pragma endregion ImagePlatformImpl

#pragma region Image

template <>
Image<Platform::VULKAN>::Image(const TextureDesc &texture_desc)
    : m_platform_impl { this },
      m_texture_desc(texture_desc),
      m_is_blended(false),
      m_is_rw_texture(false),
      m_is_attachment_texture(false),
      m_bpp(NumComponents(GetBaseFormat(texture_desc.format)))
{
    m_size = GetByteSize();
}

template <>
Image<Platform::VULKAN>::Image(Image &&other) noexcept
    : m_texture_desc(other.m_texture_desc),
      m_is_blended(other.m_is_blended),
      m_is_rw_texture(other.m_is_rw_texture),
      m_is_attachment_texture(other.m_is_attachment_texture),
      m_size(other.m_size),
      m_bpp(other.m_bpp)
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;
}

template <>
Image<Platform::VULKAN> &Image<Platform::VULKAN>::operator=(Image &&other) noexcept
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;
    other.m_platform_impl = { &other };

    m_texture_desc = std::move(other.m_texture_desc);
    m_is_blended = other.m_is_blended;
    m_is_rw_texture = other.m_is_rw_texture;
    m_is_attachment_texture = other.m_is_attachment_texture;
    m_size = other.m_size;
    m_bpp = other.m_bpp;

    other.m_is_blended = false;
    other.m_is_rw_texture = false;
    other.m_is_attachment_texture = false;
    other.m_size = 0;
    other.m_bpp = 0;

    return *this;
}

template <>
Image<Platform::VULKAN>::~Image()
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);
}

template <>
bool Image<Platform::VULKAN>::IsCreated() const
    { return m_platform_impl.handle != VK_NULL_HANDLE; }

template <>
RendererResult Image<Platform::VULKAN>::GenerateMipmaps(
    Device<Platform::VULKAN> *device,
    CommandBuffer<Platform::VULKAN> *command_buffer
)
{
    if (m_platform_impl.handle == VK_NULL_HANDLE) {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const auto num_faces = NumFaces();
    const auto num_mipmaps = NumMipmaps();

    for (uint32 face = 0; face < num_faces; face++) {
        for (int32 i = 1; i < int32(num_mipmaps + 1); i++) {
            const auto mip_width = int32(helpers::MipmapSize(m_texture_desc.extent.x, i)),
                mip_height = int32(helpers::MipmapSize(m_texture_desc.extent.y, i)),
                mip_depth = int32(helpers::MipmapSize(m_texture_desc.extent.z, i));

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
            
            m_platform_impl.InsertSubResourceBarrier(
                command_buffer,
                src,
                ResourceState::COPY_SRC
            );

            if (i == int32(num_mipmaps)) {
                if (face == num_faces - 1) {
                    /* all individual subresources have been set so we mark the whole
                     * resource as being int his state */
                    m_platform_impl.SetResourceState(ResourceState::COPY_SRC);
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
                        int32(helpers::MipmapSize(m_texture_desc.extent.x, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.y, i - 1)),
                        int32(helpers::MipmapSize(m_texture_desc.extent.z, i - 1))
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
                command_buffer->GetPlatformImpl().command_buffer,
                m_platform_impl.handle,
                GetVkImageLayout(ResourceState::COPY_SRC),
                m_platform_impl.handle,
                GetVkImageLayout(ResourceState::COPY_DST),
                1, &blit,
                IsDepthStencil() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR // TODO: base on filter mode
            );
        }
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult Image<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    RendererResult result;

    if (IsCreated()) {
        HYPERION_PASS_ERRORS(m_platform_impl.Destroy(device), result);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    return result;
}

template <>
RendererResult Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    UniquePtr<TextureData> new_texture_data;
    VkImageCreateInfo image_info;

    return m_platform_impl.Create(device, nullptr, VK_IMAGE_LAYOUT_UNDEFINED, &image_info, new_texture_data);
}

template <>
RendererResult Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, ResourceState state, const TextureData &texture_data)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    RendererResult result;

    UniquePtr<TextureData> new_texture_data;
    VkImageCreateInfo image_info;

    HYPERION_BUBBLE_ERRORS(m_platform_impl.Create(device, &texture_data, VK_IMAGE_LAYOUT_UNDEFINED, &image_info, new_texture_data));

    const ImageSubResource sub_resource {
        .num_layers = NumFaces(),
        .num_levels = NumMipmaps()
    };

    SingleTimeCommands<Platform::VULKAN> commands { device };

    GPUBufferRef<Platform::VULKAN> staging_buffer;

    const TextureData *texture_data_ptr = new_texture_data != nullptr
        ? new_texture_data.Get()
        : &texture_data;

    AssertThrowMsg(
        m_size == texture_data_ptr->buffer.Size(),
        "Invalid image size --  image size (%llu) does not match loaded data size (%llu)",
        m_size,
        texture_data_ptr->buffer.Size()
    );

    AssertThrowMsg(m_size % m_bpp == 0, "Invalid image size");
    AssertThrowMsg((m_size / m_bpp) % NumFaces() == 0, "Invalid image size");

    staging_buffer = MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::STAGING_BUFFER);
    HYPERION_PASS_ERRORS(staging_buffer->Create(device, m_size), result);

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;            
    }

    staging_buffer->Copy(device, m_size, texture_data_ptr->buffer.Data());

    new_texture_data.Reset(); // We don't need it anymore here, release the memory

    commands.Push([&](const CommandBufferRef<Platform::VULKAN> &command_buffer)
    {
        m_platform_impl.InsertBarrier(
            command_buffer,
            sub_resource,
            ResourceState::COPY_DST
        );

        HYPERION_RETURN_OK;
    });

    // copy from staging to image
    const uint32 buffer_offset_step = uint32(m_size) / NumFaces();

    AssertThrowMsg(m_size % buffer_offset_step == 0, "Invalid image size");
    AssertThrowMsg(m_size / buffer_offset_step == NumFaces(), "Invalid image size");

    for (uint32 face_index = 0; face_index < NumFaces(); face_index++) {
        commands.Push([this, &staging_buffer, &image_info, face_index, buffer_offset_step](const CommandBufferRef<Platform::VULKAN> &command_buffer)
        {
            const uint32 buffer_size = staging_buffer->Size();
            const uint32 total_size = m_size;

            VkBufferImageCopy region { };
            region.bufferOffset = face_index * buffer_offset_step;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face_index;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = image_info.extent;

            vkCmdCopyBufferToImage(
                command_buffer->GetPlatformImpl().command_buffer,
                staging_buffer->GetPlatformImpl().handle,
                m_platform_impl.handle,
                GetVkImageLayout(m_platform_impl.GetResourceState()),
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
        commands.Push([this, device](const CommandBufferRef<Platform::VULKAN> &command_buffer)
        {
            return GenerateMipmaps(device, command_buffer);
        });
    }

    /* Transition from whatever the previous layout state was to our destination state */
    commands.Push([&](const CommandBufferRef<Platform::VULKAN> &command_buffer)
    {
        m_platform_impl.InsertBarrier(
            command_buffer,
            sub_resource,
            state
        );

        HYPERION_RETURN_OK;
    });

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(), result);

    SafeRelease(std::move(staging_buffer));

    return result;
}

template <>
RendererResult Image<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, ResourceState initial_state)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }
    
    TextureData texture_data;
    texture_data.desc = m_texture_desc;
    texture_data.buffer.SetSize(m_size); // Fill with zeros

    return Create(device, initial_state, texture_data);
}

template <>
ResourceState Image<Platform::VULKAN>::GetResourceState() const
    { return m_platform_impl.GetResourceState(); }

template <>
void Image<Platform::VULKAN>::SetResourceState(ResourceState new_state)
    { m_platform_impl.SetResourceState(new_state); }

template <>
ResourceState Image<Platform::VULKAN>::GetSubResourceState(const ImageSubResource &sub_resource) const
    { return m_platform_impl.GetSubResourceState(sub_resource); }

template <>
void Image<Platform::VULKAN>::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
    { m_platform_impl.SetSubResourceState(sub_resource, new_state); }

template <>
void Image<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state
)
{
    ImageSubResourceFlagBits flags = IMAGE_SUB_RESOURCE_FLAGS_NONE;

    if (IsDepthStencil()) {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL;
    } else {
        flags |= IMAGE_SUB_RESOURCE_FLAGS_COLOR;
    }

    m_platform_impl.InsertBarrier(command_buffer, new_state, flags);
}

template <>
void Image<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    m_platform_impl.InsertBarrier(command_buffer, sub_resource, new_state);
}

template <>
void Image<Platform::VULKAN>::InsertSubResourceBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    m_platform_impl.InsertSubResourceBarrier(command_buffer, sub_resource, new_state);
}

template <>
RendererResult Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect,
    uint32 src_mip,
    uint32 dst_mip
)
{
    const uint32 num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer   = face,
            .base_mip_level     = src_mip
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
            command_buffer->GetPlatformImpl().command_buffer,
            src_image->m_platform_impl.handle,
            GetVkImageLayout(src_image->m_platform_impl.GetResourceState()),
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src_image,
    Rect<uint32> src_rect,
    Rect<uint32> dst_rect
)
{
    const uint32 num_faces = MathUtil::Min(NumFaces(), src_image->NumFaces());

    for (uint32 face = 0; face < num_faces; face++) {
        const ImageSubResource src {
            .flags = src_image->IsDepthStencil()
                ? IMAGE_SUB_RESOURCE_FLAGS_DEPTH | IMAGE_SUB_RESOURCE_FLAGS_STENCIL
                : IMAGE_SUB_RESOURCE_FLAGS_COLOR,
            .base_array_layer   = face,
            .base_mip_level     = 0
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
            command_buffer->GetPlatformImpl().command_buffer,
            src_image->m_platform_impl.handle,
            GetVkImageLayout(src_image->m_platform_impl.GetResourceState()),
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1, &blit,
            helpers::ToVkFilter(GetMinFilterMode())
        );
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult Image<Platform::VULKAN>::Blit(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Image *src
)
{
    return Blit(
        command_buffer,
        src,
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = src->GetExtent().x,
            .y1 = src->GetExtent().y
        },
        Rect<uint32> {
            .x0 = 0,
            .y0 = 0,
            .x1 = m_texture_desc.extent.x,
            .y1 = m_texture_desc.extent.y
        }
    );
}

template <>
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
    const auto buffer_offset_step = uint32(m_size) / num_faces;

    for (uint32 i = 0; i < num_faces; i++) {
        VkBufferImageCopy region { };
        region.bufferOffset = i * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = i;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_texture_desc.extent.x, m_texture_desc.extent.y, m_texture_desc.extent.z };

        vkCmdCopyBufferToImage(
            command_buffer->GetPlatformImpl().command_buffer,
            src_buffer->GetPlatformImpl().handle,
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            1,
            &region
        );
    }
}

template <>
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
    const uint32 num_faces = NumFaces();
    const uint32 buffer_offset_step = uint32(m_size) / num_faces;

    for (uint32 face_index = 0; face_index < num_faces; face_index++) {
        VkBufferImageCopy region { };
        region.bufferOffset = face_index * buffer_offset_step;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspect_flag_bits;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = face_index;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = VkExtent3D { m_texture_desc.extent.x, m_texture_desc.extent.y, m_texture_desc.extent.z };

        vkCmdCopyImageToBuffer(
            command_buffer->GetPlatformImpl().command_buffer,
            m_platform_impl.handle,
            GetVkImageLayout(m_platform_impl.GetResourceState()),
            dst_buffer->GetPlatformImpl().handle,
            1,
            &region
        );
    }
}

template <>
ByteBuffer Image<Platform::VULKAN>::ReadBack(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance) const
{
    if (m_size == 0) {
        return ByteBuffer(0, nullptr);
    }

    StagingBuffer<Platform::VULKAN> staging_buffer;

    SingleTimeCommands<Platform::VULKAN> commands { device };

    RendererResult result = RendererResult { };

    HYPERION_PASS_ERRORS(staging_buffer.Create(device, m_size), result);

    if (!result) {
        return ByteBuffer(0, nullptr);
    }

    commands.Push([non_const_this = const_cast<Image *>(this), &staging_buffer](const CommandBufferRef<Platform::VULKAN> &command_buffer)
    {
        const ResourceState previous_resource_state = non_const_this->GetResourceState();

        non_const_this->InsertBarrier(command_buffer.Get(), ResourceState::COPY_SRC);
        non_const_this->CopyToBuffer(command_buffer, &staging_buffer);
        non_const_this->InsertBarrier(command_buffer.Get(), previous_resource_state);

        HYPERION_RETURN_OK;
    });

    // execute command stack
    HYPERION_PASS_ERRORS(commands.Execute(), result);

    ByteBuffer byte_buffer(m_size);
    staging_buffer.Read(device, m_size, byte_buffer.Data());

    if (result) {
        // destroy staging buffer
        HYPERION_PASS_ERRORS(staging_buffer.Destroy(device), result);
    } else {
        HYPERION_IGNORE_ERRORS(staging_buffer.Destroy(device));
    }

    return byte_buffer;
}

#pragma endregion Image

} // namespace platform
} // namespace renderer
} // namespace hyperion
