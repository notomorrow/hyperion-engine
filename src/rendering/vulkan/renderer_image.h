#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_buffer.h"
#include "../texture.h"

namespace hyperion {
class RendererPipeline;
class RendererImage {
public:
    struct LayoutTransferStateBase;

    RendererImage(size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        VkImageTiling tiling,
        VkImageUsageFlags image_usage_flags,
        unsigned char *bytes);

    RendererImage(const RendererImage &other) = delete;
    RendererImage &operator=(const RendererImage &other) = delete;
    ~RendererImage();

    RendererResult Create(RendererDevice *device, VkImageLayout layout);
    RendererResult Create(RendererDevice *device, RendererPipeline *pipeline,
        const LayoutTransferStateBase &transfer_from,
        const LayoutTransferStateBase &transfer_to);
    RendererResult Destroy(RendererDevice *device);

    inline RendererGPUImage *GetGPUImage() { return m_image; }
    inline const RendererGPUImage *GetGPUImage() const { return m_image; }

    inline Texture::TextureInternalFormat GetTextureFormat() const { return m_format; }
    inline Texture::TextureType GetTextureType() const { return m_type; }

    inline VkFormat GetImageFormat() const { return ToVkFormat(m_format); }
    inline VkImageType GetImageType() const { return ToVkType(m_type); }
    inline VkImageUsageFlags GetImageUsageFlags() const { return m_image_usage_flags; }

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

    static VkFormat ToVkFormat(Texture::TextureInternalFormat);
    static VkImageType ToVkType(Texture::TextureType);

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
    VkImageTiling m_tiling;
    VkImageUsageFlags m_image_usage_flags;
    unsigned char *m_bytes;

    size_t m_size;
    size_t m_bpp; // bytes per pixel
    RendererStagingBuffer *m_staging_buffer;
    RendererGPUImage *m_image;
};

/*class RendererSampledImage : public RendererImage {
public:
    RendererSampledImage(
        size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        VkImageTiling tiling,
        VkImageUsageFlags image_usage_flags,
        unsigned char *bytes
    ) : RendererImage(
        width, height, depth,
        format, type,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
};*/

} // namespace hyperion

#endif