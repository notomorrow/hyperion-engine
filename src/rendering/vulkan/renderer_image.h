#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_buffer.h"
#include "renderer_helpers.h"
#include "../texture.h"

namespace hyperion {
class VkRenderer;
class RendererImage {
public:
    struct LayoutTransferStateBase;

    struct InternalInfo {
        VkImageTiling tiling;
        VkImageUsageFlags usage_flags;
    };

    RendererImage(size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        const InternalInfo &internal_info,
        unsigned char *bytes);

    RendererImage(const RendererImage &other) = delete;
    RendererImage &operator=(const RendererImage &other) = delete;
    ~RendererImage();

    RendererResult Create(RendererDevice *device, VkImageLayout layout);
    RendererResult Create(RendererDevice *device, VkRenderer *renderer,
        const LayoutTransferStateBase &transfer_from,
        const LayoutTransferStateBase &transfer_to);
    RendererResult Destroy(RendererDevice *device);

    bool IsDepthStencilImage() const;

    inline RendererGPUImage *GetGPUImage() { return m_image; }
    inline const RendererGPUImage *GetGPUImage() const { return m_image; }

    inline Texture::TextureInternalFormat GetTextureFormat() const { return m_format; }
    inline Texture::TextureType GetTextureType() const { return m_type; }

    inline VkFormat GetImageFormat() const { return helpers::ToVkFormat(m_format); }
    inline VkImageType GetImageType() const { return helpers::ToVkType(m_type); }
    inline VkImageUsageFlags GetImageUsageFlags() const { return m_internal_info.usage_flags; }

    struct LayoutTransferStateBase {
        VkImageLayout old_layout;
        VkImageLayout new_layout;
        VkAccessFlags src_access_mask;
        VkAccessFlags dst_access_mask;
        VkPipelineStageFlags src_stage_mask;
        VkPipelineStageFlags dst_stage_mask;
    };

    template <VkImageLayout OldLayout, VkImageLayout NewLayout>
    struct LayoutTransferState : public LayoutTransferStateBase {
        LayoutTransferState() = delete;
        LayoutTransferState(const LayoutTransferState &other) = default;
    };

    /* layout transfer specialization structs */

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    > : public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .src_access_mask = VK_IMAGE_LAYOUT_UNDEFINED,
            .dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT
        } {}
    };

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    > : public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .src_access_mask = VK_IMAGE_LAYOUT_UNDEFINED,
            .dst_access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
        } {}
    };

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    >: public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dst_access_mask = VK_ACCESS_SHADER_READ_BIT,
            .src_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .dst_stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        } {}
    };


private:
    RendererResult CreateImage(RendererDevice *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info);

    RendererResult ConvertTo32Bpp(RendererDevice *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format);

    size_t m_width;
    size_t m_height;
    size_t m_depth;
    Texture::TextureInternalFormat m_format;
    Texture::TextureType m_type;
    unsigned char *m_bytes;

    InternalInfo m_internal_info;

    size_t m_size;
    size_t m_bpp; // bytes per pixel
    RendererStagingBuffer *m_staging_buffer;
    RendererGPUImage *m_image;
};
} // namespace hyperion

#endif