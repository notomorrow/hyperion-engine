#ifndef RENDERER_IMAGE_H
#define RENDERER_IMAGE_H

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

using v2::StreamedData;
using v2::MemoryStreamedData;

class Instance;
class Device;

using ImageFlags = UInt32;

enum ImageFlagBits : ImageFlags
{
    IMAGE_FLAGS_NONE = 0x0,
    IMAGE_FLAGS_KEEP_IMAGE_DATA = 0x1
};

class Image
{
public:
    struct InternalInfo
    {
        VkImageTiling tiling;
        VkImageUsageFlags usage_flags;
    };

    Image(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        const InternalInfo &internal_info,
        UniquePtr<StreamedData> &&streamed_data,
        ImageFlags flags = IMAGE_FLAGS_NONE
    );

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other) noexcept;
    ~Image();

    /*
     * Create the image. No texture data will be copied.
     */
    Result Create(Device *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    Result Create(Device *device, Instance *instance, ResourceState state);
    Result Destroy(Device *device);

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src
    );

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src,
        Rect src_rect,
        Rect dst_rect
    );

    Result Blit(
        CommandBuffer *command_buffer,
        const Image *src,
        Rect src_rect,
        Rect dst_rect,
        UInt src_mip,
        UInt dst_mip
    );

    Result GenerateMipmaps(
        Device *device,
        CommandBuffer *command_buffer
    );

    void CopyFromBuffer(
        CommandBuffer *command_buffer,
        const GPUBuffer *src_buffer
    ) const;

    void CopyToBuffer(
        CommandBuffer *command_buffer,
        GPUBuffer *dst_buffer
    ) const;

    ByteBuffer ReadBack(Device *device, Instance *instance) const;
    
    const StreamedData *GetStreamedData() const
        { return m_streamed_data.Get(); }

    bool HasAssignedImageData() const
        { return m_streamed_data != nullptr && !m_streamed_data->IsNull(); }

    void CopyImageData(const ByteBuffer &byte_buffer)
    {
        m_streamed_data.Reset(new MemoryStreamedData(byte_buffer));
    }

    void CopyImageData(const UByte *data, SizeType count, SizeType offset = 0)
    {
        m_streamed_data.Reset(new MemoryStreamedData(ByteBuffer(count, data + offset)));
    }

    bool IsDepthStencil() const;
    bool IsSRGB() const;
    void SetIsSRGB(bool srgb);

    bool IsBlended() const
        { return m_is_blended; }

    void SetIsBlended(bool is_blended)
        { m_is_blended = is_blended; }

    bool HasMipmaps() const
    {
        return m_filter_mode == FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP
            || m_filter_mode == FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
            || m_filter_mode == FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP;
    }

    UInt NumMipmaps() const
    {
        return HasMipmaps()
            ? static_cast<UInt>(MathUtil::FastLog2(MathUtil::Max(m_extent.width, m_extent.height, m_extent.depth))) + 1
            : 1;
    }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    SizeType GetByteSize() const
        { return static_cast<SizeType>(m_extent.Size())
            * static_cast<SizeType>(NumComponents(m_format))
            * static_cast<SizeType>(NumFaces()); }

    bool IsTextureCube() const
        { return m_type == ImageType::TEXTURE_TYPE_CUBEMAP; }

    bool IsPanorama() const
        { return m_type == ImageType::TEXTURE_TYPE_2D
            && m_extent.width == m_extent.height * 2
            && m_extent.depth == 1; }

    UInt NumFaces() const
        { return IsTextureCube() ? 6 : 1; }

    FilterMode GetFilterMode() const
        { return m_filter_mode; }

    void SetFilterMode(FilterMode filter_mode)
        { m_filter_mode = filter_mode; }

    const Extent3D &GetExtent() const
        { return m_extent; }

    GPUImageMemory *GetGPUImage()
        { return m_image; }

    const GPUImageMemory *GetGPUImage() const
        { return m_image; }

    InternalFormat GetTextureFormat() const
        { return m_format; }

    void SetTextureFormat(InternalFormat format)
        { m_format = format; }

    ImageType GetType() const
        { return m_type; }

protected:
    ImageFlags m_flags;
    
private:
    Result CreateImage(
        Device *device,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info
    );

    Result ConvertTo32BPP(
        Device *device,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format
    );

    Extent3D m_extent;
    InternalFormat m_format;
    ImageType m_type;
    FilterMode m_filter_mode;
    UniquePtr<StreamedData> m_streamed_data;

    bool m_is_blended;

    InternalInfo m_internal_info;

    SizeType m_size;
    SizeType m_bpp; // bytes per pixel
    GPUImageMemory *m_image;
};

class StorageImage : public Image
{
public:
    StorageImage()
        : StorageImage(
            Extent3D { 1, 1, 1 },
            InternalFormat::RGBA16F,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_NEAREST,
            nullptr
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            std::move(streamed_data)
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
                | VK_IMAGE_USAGE_STORAGE_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT
        },
        std::move(streamed_data)
    )
    {
    }

    StorageImage(const StorageImage &other) = delete;
    StorageImage &operator=(const StorageImage &other) = delete;

    StorageImage(StorageImage &&other) noexcept
        : Image(std::move(other))
    {
    }

    StorageImage &operator=(StorageImage &&other) noexcept
    {
        Image::operator=(std::move(other));

        return *this;
    }

    ~StorageImage() = default;
};

class StorageImage2D : public StorageImage
{
public:
    StorageImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        std::move(streamed_data)
    )
    {
    }
    
    StorageImage2D(const StorageImage2D &other) = delete;
    StorageImage2D &operator=(const StorageImage2D &other) = delete;

    StorageImage2D(StorageImage2D &&other) noexcept
        : StorageImage(std::move(other))
    {
    }

    StorageImage2D &operator=(StorageImage2D &&other) noexcept
    {
        StorageImage::operator=(std::move(other));

        return *this;
    }

    ~StorageImage2D() = default;
};

class StorageImage3D : public StorageImage
{
public:
    StorageImage3D(
        Extent3D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data = nullptr
    ) : StorageImage(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        std::move(streamed_data)
    )
    {
    }

    StorageImage3D(const StorageImage3D &other) = delete;
    StorageImage3D &operator=(const StorageImage3D &other) = delete;

    StorageImage3D(StorageImage3D &&other) noexcept
        : StorageImage(std::move(other))
    {
    }

    StorageImage3D &operator=(StorageImage3D &&other) noexcept
    {
        StorageImage::operator=(std::move(other));

        return *this;
    }

    ~StorageImage3D() = default;
};

class TextureImage : public Image
{
public:
    TextureImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo{
            .tiling      = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                         | VK_IMAGE_USAGE_SAMPLED_BIT
        },
        std::move(streamed_data)
    )
    {
    }

    TextureImage(const TextureImage &other) = delete;
    TextureImage &operator=(const TextureImage &other) = delete;

    TextureImage(TextureImage &&other) noexcept
        : Image(std::move(other))
    {
    }

    TextureImage &operator=(TextureImage &&other) noexcept
    {
        Image::operator=(std::move(other));

        return *this;
    }

    ~TextureImage() = default;
};

class TextureImage2D : public TextureImage
{
public:
    TextureImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_2D,
        filter_mode,
        std::move(streamed_data)
    )
    {
    }

    TextureImage2D(const TextureImage2D &other) = delete;
    TextureImage2D &operator=(const TextureImage2D &other) = delete;

    TextureImage2D(TextureImage2D &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImage2D &operator=(TextureImage2D &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImage2D() = default;
};

class TextureImage3D : public TextureImage
{
public:
    TextureImage3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
        extent,
        format,
        ImageType::TEXTURE_TYPE_3D,
        filter_mode,
        std::move(streamed_data)
    )
    {
    }

    TextureImage3D(const TextureImage3D &other) = delete;
    TextureImage3D &operator=(const TextureImage3D &other) = delete;

    TextureImage3D(TextureImage3D &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImage3D &operator=(TextureImage3D &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImage3D() = default;
};

class TextureImageCube : public TextureImage
{
public:
    TextureImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode,
        UniquePtr<StreamedData> &&streamed_data
    ) : TextureImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        filter_mode,
        std::move(streamed_data)
    )
    {
    }

    TextureImageCube(const TextureImageCube &other) = delete;
    TextureImageCube &operator=(const TextureImageCube &other) = delete;

    TextureImageCube(TextureImageCube &&other) noexcept
        : TextureImage(std::move(other))
    {
    }

    TextureImageCube &operator=(TextureImageCube &&other) noexcept
    {
        TextureImage::operator=(std::move(other));

        return *this;
    }

    ~TextureImageCube() = default;
};

class FramebufferImage : public Image
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        UniquePtr<StreamedData> &&streamed_data
    ) : Image(
        extent,
        format,
        type,
        FilterMode::TEXTURE_FILTER_NEAREST,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VkImageUsageFlags((GetBaseFormat(format) == BaseFormat::TEXTURE_FORMAT_DEPTH
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                | VK_IMAGE_USAGE_SAMPLED_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT /* for mip chain */)
        },
        std::move(streamed_data)
    )
    {
    }

    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode filter_mode
    ) : Image(
        extent,
        format,
        type,
        filter_mode,
        Image::InternalInfo {
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage_flags = VkImageUsageFlags((GetBaseFormat(format) == BaseFormat::TEXTURE_FORMAT_DEPTH
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                | VK_IMAGE_USAGE_SAMPLED_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT /* for mip chain */)
        },
        nullptr
    )
    {
    }
};

class FramebufferImage2D : public FramebufferImage
{
public:
    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            std::move(streamed_data)
        )
    {
    }

    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode
    ) : FramebufferImage(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            filter_mode
        )
    {
    }
};

class FramebufferImageCube : public FramebufferImage
{
public:
    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        UniquePtr<StreamedData> &&streamed_data
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        std::move(streamed_data)
    )
    {
    }

    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode filter_mode
    ) : FramebufferImage(
        Extent3D(extent),
        format,
        ImageType::TEXTURE_TYPE_CUBEMAP,
        filter_mode
    )
    {
    }
};


} // namespace renderer
} // namespace hyperion

#endif
