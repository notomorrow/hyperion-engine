#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include "renderer_result.h"
#include "renderer_device.h"
#include "renderer_buffer.h"
#include "renderer_helpers.h"
#include "../texture.h"

#include <math/math_util.h>

namespace hyperion {
namespace renderer {
class Instance;
class Image {
public:
    struct LayoutTransferStateBase;

    struct InternalInfo {
        VkImageTiling tiling;
        VkImageUsageFlags usage_flags;
    };

    Image(size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        Texture::TextureFilterMode filter_mode,
        const InternalInfo &internal_info,
        unsigned char *bytes);

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;
    ~Image();

    Result Create(Device *device, VkImageLayout layout);
    Result Create(Device *device, Instance *renderer,
        LayoutTransferStateBase transfer_from,
        LayoutTransferStateBase transfer_to);
    Result Destroy(Device *device);

    inline bool IsDepthStencilImage() const
        { return helpers::IsDepthTexture(m_format); }

    inline bool IsMipmappedImage() const
        { return m_filter_mode == Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP; }

    inline size_t GetNumMipmaps() const
    {
        return IsMipmappedImage()
            ? MathUtil::FastLog2(MathUtil::Max(m_width, m_height, m_depth)) + 1
            : 1;
    }

    inline bool IsCubemap() const
        { return m_type == Texture::TextureType::TEXTURE_TYPE_CUBEMAP; }

    inline size_t GetNumFaces() const
        { return IsCubemap() ? 6 : 1; }

    inline GPUImage *GetGPUImage() { return m_image; }
    inline const GPUImage *GetGPUImage() const { return m_image; }

    inline Texture::TextureInternalFormat GetTextureFormat() const { return m_format; }
    inline Texture::TextureType GetTextureType() const { return m_type; }

    inline VkFormat GetImageFormat() const { return helpers::ToVkFormat(m_format); }
    inline VkImageType GetImageType() const { return helpers::ToVkType(m_type); }
    inline VkImageUsageFlags GetImageUsageFlags() const { return m_internal_info.usage_flags; }

    struct LayoutState {
        VkImageLayout layout;
        VkAccessFlags access_mask;
        VkPipelineStageFlags stage_mask;
    };

    struct LayoutTransferStateBase {
        LayoutState src;
        LayoutState dst;
    };

    template <VkImageLayout OldLayout, VkImageLayout NewLayout>
    struct LayoutTransferState : public LayoutTransferStateBase {
        LayoutTransferState() = delete;
    };

    /* layout transfer specialization structs */

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    > : public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .src = {
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .access_mask = VK_IMAGE_LAYOUT_UNDEFINED,
                .stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            },
            .dst = {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT
            }
        } {}
    };

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    > : public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .src = {
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .access_mask = VK_IMAGE_LAYOUT_UNDEFINED,
                .stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            },
            .dst = {
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .access_mask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .stage_mask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
            }
        } {}
    };

    template<>
    struct LayoutTransferState<
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    >: public LayoutTransferStateBase {
        LayoutTransferState() : LayoutTransferStateBase{
            .src = {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .access_mask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT
            },
            .dst = {
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .access_mask = VK_ACCESS_SHADER_READ_BIT,
                .stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            }
        } {}
    };

private:
    Result CreateImage(Device *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info);

    Result ConvertTo32Bpp(Device *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format);

    size_t m_width;
    size_t m_height;
    size_t m_depth;
    Texture::TextureInternalFormat m_format;
    Texture::TextureType m_type;
    Texture::TextureFilterMode m_filter_mode;
    unsigned char *m_bytes;

    InternalInfo m_internal_info;

    size_t m_size;
    size_t m_bpp; // bytes per pixel
    StagingBuffer *m_staging_buffer;
    GPUImage *m_image;
};

class TextureImage : public Image {
public:
    TextureImage(
        size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        Texture::TextureFilterMode filter_mode,
        unsigned char *bytes
    ) : Image(
        width,
        height,
        depth,
        format,
        type,
        filter_mode,
        Image::InternalInfo{
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        },
        bytes
    ) {}
};

class TextureImage2D : public TextureImage {
public:
    TextureImage2D(
        size_t width, size_t height,
        Texture::TextureInternalFormat format,
        Texture::TextureFilterMode filter_mode,
        unsigned char *bytes
    ) : TextureImage(
        width,
        height,
        1,
        format,
        Texture::TextureType::TEXTURE_TYPE_2D,
        filter_mode,
        bytes
    ) {}
};

class TextureImage3D : public TextureImage {
public:
    TextureImage3D(
        size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureFilterMode filter_mode,
        unsigned char *bytes
    ) : TextureImage(
        width,
        height,
        depth,
        format,
        Texture::TextureType::TEXTURE_TYPE_3D,
        filter_mode,
        bytes
    ) {}
};

class TextureImageCubemap : public TextureImage {
public:
    TextureImageCubemap(
        size_t width, size_t height,
        Texture::TextureInternalFormat format,
        Texture::TextureFilterMode filter_mode,
        unsigned char *bytes
    ) : TextureImage(
        width,
        height,
        1,
        format,
        Texture::TextureType::TEXTURE_TYPE_CUBEMAP,
        filter_mode,
        bytes
    ) {}
};

class FramebufferImage : public Image {
public:
    FramebufferImage(
        size_t width, size_t height, size_t depth,
        Texture::TextureInternalFormat format,
        Texture::TextureType type,
        unsigned char *bytes
    ) : Image(
        width,
        height,
        depth,
        format,
        type,
        Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST,
        Image::InternalInfo{
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VkImageUsageFlags(((Texture::GetBaseFormat(format) == Texture::TextureBaseFormat::TEXTURE_FORMAT_DEPTH)
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                | VK_IMAGE_USAGE_SAMPLED_BIT)
        },
        bytes
    ) {}
};

class FramebufferImage2D : public FramebufferImage {
public:
    FramebufferImage2D(
        size_t width, size_t height,
        Texture::TextureInternalFormat format,
        unsigned char *bytes
    ) : FramebufferImage(
        width,
        height,
        1,
        format,
        Texture::TextureType::TEXTURE_TYPE_2D,
        bytes
    ) {}
};


} // namespace renderer
} // namespace hyperion

#endif