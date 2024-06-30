/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_IMAGE_HPP
#define HYPERION_BACKEND_RENDERER_IMAGE_HPP

#include <core/Defines.hpp>

#include <math/MathUtil.hpp>
#include <math/Rect.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

static inline BaseFormat GetBaseFormat(InternalFormat fmt)
{
    switch (fmt) {
    case InternalFormat::R8:
    case InternalFormat::R8_SRGB:
    case InternalFormat::R32_:
    case InternalFormat::R16:
    case InternalFormat::R32:
    case InternalFormat::R16F:
    case InternalFormat::R32F:
        return BaseFormat::TEXTURE_FORMAT_R;
    case InternalFormat::RG8:
    case InternalFormat::RG8_SRGB:
    case InternalFormat::RG16_:
    case InternalFormat::RG16:
    case InternalFormat::RG32:
    case InternalFormat::RG16F:
    case InternalFormat::RG32F:
        return BaseFormat::TEXTURE_FORMAT_RG;
    case InternalFormat::RGB8:
    case InternalFormat::RGB8_SRGB:
    case InternalFormat::R11G11B10F:
    case InternalFormat::RGB16:
    case InternalFormat::RGB32:
    case InternalFormat::RGB16F:
    case InternalFormat::RGB32F:
        return BaseFormat::TEXTURE_FORMAT_RGB;
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA8_SRGB:
    case InternalFormat::R10G10B10A2:
    case InternalFormat::RGBA16:
    case InternalFormat::RGBA32:
    case InternalFormat::RGBA16F:
    case InternalFormat::RGBA32F:
        return BaseFormat::TEXTURE_FORMAT_RGBA;
    case InternalFormat::BGR8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGR;
    case InternalFormat::BGRA8:
    case InternalFormat::BGRA8_SRGB:
        return BaseFormat::TEXTURE_FORMAT_BGRA;
    case InternalFormat::DEPTH_16:
    case InternalFormat::DEPTH_24:
    case InternalFormat::DEPTH_32F:
        return BaseFormat::TEXTURE_FORMAT_DEPTH;
    default:
        // undefined result
        return BaseFormat::TEXTURE_FORMAT_NONE;
    }
}

static inline uint32 NumComponents(BaseFormat format)
{
    switch (format) {
    case BaseFormat::TEXTURE_FORMAT_NONE: return 0;
    case BaseFormat::TEXTURE_FORMAT_R: return 1;
    case BaseFormat::TEXTURE_FORMAT_RG: return 2;
    case BaseFormat::TEXTURE_FORMAT_RGB: return 3;
    case BaseFormat::TEXTURE_FORMAT_BGR: return 3;
    case BaseFormat::TEXTURE_FORMAT_RGBA: return 4;
    case BaseFormat::TEXTURE_FORMAT_BGRA: return 4;
    case BaseFormat::TEXTURE_FORMAT_DEPTH: return 1;
    default: return 0; // undefined result
    }
}

static inline uint32 NumComponents(InternalFormat format)
{
    return NumComponents(GetBaseFormat(format));
}

static inline uint32 NumBytes(InternalFormat format)
{
    switch (format) {
    case InternalFormat::R8:
    case InternalFormat::R8_SRGB:
    case InternalFormat::RG8:
    case InternalFormat::RG8_SRGB:
    case InternalFormat::RGB8:
    case InternalFormat::RGB8_SRGB:
    case InternalFormat::BGR8_SRGB:
    case InternalFormat::RGBA8:
    case InternalFormat::RGBA8_SRGB:
    case InternalFormat::BGRA8:
    case InternalFormat::BGRA8_SRGB:
        return 1;
    case InternalFormat::R16:
    case InternalFormat::RG16:
    case InternalFormat::RGB16:
    case InternalFormat::RGBA16:
    case InternalFormat::DEPTH_16:
        return 2;
    case InternalFormat::R32:
    case InternalFormat::RG32:
    case InternalFormat::RGB32:
    case InternalFormat::RGBA32:
    case InternalFormat::R32_:
    case InternalFormat::RG16_:
    case InternalFormat::R11G11B10F:
    case InternalFormat::R10G10B10A2:
    case InternalFormat::DEPTH_24:
    case InternalFormat::DEPTH_32F:
        return 4;
    case InternalFormat::R16F:
    case InternalFormat::RG16F:
    case InternalFormat::RGB16F:
    case InternalFormat::RGBA16F:
        return 2;
    case InternalFormat::R32F:
    case InternalFormat::RG32F:
    case InternalFormat::RGB32F:
    case InternalFormat::RGBA32F:
        return 4;
    default:
        return 0; // undefined result
    }
}

/*! \brief returns a texture format that has a shifted bytes-per-pixel count
 * e.g calling with RGB16 and num components = 4 --> RGBA16 */
static inline InternalFormat FormatChangeNumComponents(InternalFormat fmt, uint8 new_num_components)
{
    if (new_num_components == 0) {
        return InternalFormat::NONE;
    }

    new_num_components = MathUtil::Clamp(new_num_components, static_cast<uint8>(1), static_cast<uint8>(4));

    int current_num_components = int(NumComponents(fmt));

    return InternalFormat(int(fmt) + int(new_num_components) - current_num_components);
}

static inline bool IsDepthFormat(BaseFormat fmt)
{
    return fmt == BaseFormat::TEXTURE_FORMAT_DEPTH;
}

static inline bool IsDepthFormat(InternalFormat fmt)
{
    return IsDepthFormat(GetBaseFormat(fmt));
}

static inline bool IsSRGBFormat(InternalFormat fmt)
{
    return fmt >= InternalFormat::SRGB && fmt < InternalFormat::DEPTH;
}

namespace platform {

template <PlatformType PLATFORM>
struct ImagePlatformImpl;

template <PlatformType PLATFORM>
class Image
{
public:
    friend struct ImagePlatformImpl<PLATFORM>;

    static constexpr PlatformType platform = PLATFORM;
    
    HYP_API Image(const TextureDesc &texture_desc);
    
    HYP_API Image(const RC<StreamedTextureData> &streamed_data);
    HYP_API Image(RC<StreamedTextureData> &&streamed_data);

    Image(const Image &other)               = delete;
    Image &operator=(const Image &other)    = delete;
    HYP_API Image(Image &&other) noexcept;
    HYP_API Image &operator=(Image &&other) noexcept;
    HYP_API ~Image();

    HYP_NODISCARD HYP_FORCE_INLINE
    ImagePlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const ImagePlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const TextureDesc &GetTextureDesc() const
        { return m_texture_desc; }

    /*
     * Create the image. No texture data will be copied.
     */
    HYP_API Result Create(Device<PLATFORM> *device);

    /* Create the image and transfer the provided texture data into it if given.
     * The image is transitioned into the given state.
     */
    HYP_API Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance, ResourceState state);
    HYP_API Result Destroy(Device<PLATFORM> *device);

    HYP_NODISCARD HYP_API bool IsCreated() const;

    HYP_NODISCARD HYP_API ResourceState GetResourceState() const;

    HYP_API void SetResourceState(ResourceState new_state);

    HYP_NODISCARD HYP_API ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;

    HYP_API void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        ResourceState new_state,
        ImageSubResourceFlagBits flags = ImageSubResourceFlags::IMAGE_SUB_RESOURCE_FLAGS_COLOR
    );

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API void InsertSubResourceBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect
    );

    HYP_API Result Blit(
        CommandBuffer<PLATFORM> *command_buffer,
        const Image *src,
        Rect<uint32> src_rect,
        Rect<uint32> dst_rect,
        uint32 src_mip,
        uint32 dst_mip
    );

    HYP_API Result GenerateMipmaps(
        Device<PLATFORM> *device,
        CommandBuffer<PLATFORM> *command_buffer
    );

    HYP_API void CopyFromBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        const GPUBuffer<PLATFORM> *src_buffer
    ) const;

    HYP_API void CopyToBuffer(
        CommandBuffer<PLATFORM> *command_buffer,
        GPUBuffer<PLATFORM> *dst_buffer
    ) const;

    HYP_NODISCARD HYP_API
    ByteBuffer ReadBack(Device<PLATFORM> *device, Instance<PLATFORM> *instance) const;

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsRWTexture() const
        { return m_is_rw_texture; }

    HYP_FORCE_INLINE
    void SetIsRWTexture(bool is_rw_texture)
        { m_is_rw_texture = is_rw_texture; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsAttachmentTexture() const
        { return m_is_attachment_texture; }

    HYP_FORCE_INLINE
    void SetIsAttachmentTexture(bool is_attachment_texture)
        { m_is_attachment_texture = is_attachment_texture; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const RC<StreamedTextureData> &GetStreamedData() const
        { return m_streamed_data; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasAssignedImageData() const
        { return m_streamed_data != nullptr && m_streamed_data->GetBufferSize() != 0; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsDepthStencil() const
        { return IsDepthFormat(m_texture_desc.format); }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsSRGB() const
        { return IsSRGBFormat(m_texture_desc.format); }

    HYP_FORCE_INLINE
    void SetIsSRGB(bool srgb)
    {
        const bool is_srgb = IsSRGB();

        if (srgb == is_srgb) {
            return;
        }

        const InternalFormat format = m_texture_desc.format;

        if (is_srgb) {
            m_texture_desc.format = InternalFormat(int(format) - int(InternalFormat::SRGB));

            return;
        }

        const InternalFormat to_srgb_format = InternalFormat(int(InternalFormat::SRGB) + int(format));

        if (!IsSRGBFormat(to_srgb_format)) {
            DebugLog(
                LogType::Warn,
                "No SRGB counterpart for image type (%d)\n",
                int(format)
            );
        }

        m_texture_desc.format = to_srgb_format;
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsBlended() const
        { return m_is_blended; }

    HYP_FORCE_INLINE
    void SetIsBlended(bool is_blended)
        { m_is_blended = is_blended; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool HasMipmaps() const
    {
        return m_texture_desc.filter_mode_min == FilterMode::TEXTURE_FILTER_NEAREST_MIPMAP
            || m_texture_desc.filter_mode_min == FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP
            || m_texture_desc.filter_mode_min == FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP;
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 NumMipmaps() const
    {
        return HasMipmaps()
            ? uint32(MathUtil::FastLog2(MathUtil::Max(m_texture_desc.extent.width, m_texture_desc.extent.height, m_texture_desc.extent.depth))) + 1
            : 1;
    }

    /*! \brief Returns the byte-size of the image. Note, it's possible no CPU-side memory exists
        for the image data even if the result is non-zero. To check if any CPU-side bytes exist,
        use HasAssignedImageData(). */
    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 GetByteSize() const
        { return uint32(m_texture_desc.extent.Size())
            * NumComponents(m_texture_desc.format)
            * NumBytes(m_texture_desc.format)
            * NumFaces(); }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint8 GetBPP() const
        { return m_bpp; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsTextureCube() const
        { return m_texture_desc.type == ImageType::TEXTURE_TYPE_CUBEMAP; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsPanorama() const
        { return m_texture_desc.type == ImageType::TEXTURE_TYPE_2D
            && m_texture_desc.extent.width == m_texture_desc.extent.height * 2
            && m_texture_desc.extent.depth == 1; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsTextureArray() const
        { return !IsTextureCube() && m_texture_desc.num_layers > 1; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsTexture3D() const
        { return m_texture_desc.type == ImageType::TEXTURE_TYPE_3D; }

    HYP_NODISCARD HYP_FORCE_INLINE
    bool IsTexture2D() const
        { return m_texture_desc.type == ImageType::TEXTURE_TYPE_2D; }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 NumLayers() const
        { return m_texture_desc.num_layers; }

    HYP_FORCE_INLINE
    void SetNumLayers(uint32 num_layers)
    {
        m_texture_desc.num_layers = num_layers;
        m_size = GetByteSize();
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 NumFaces() const
        { return IsTextureCube()
            ? 6
            : IsTextureArray()
                ? m_texture_desc.num_layers
                : 1; }

    HYP_NODISCARD HYP_FORCE_INLINE
    FilterMode GetMinFilterMode() const
        { return m_texture_desc.filter_mode_min; }

    HYP_FORCE_INLINE
    void SetMinFilterMode(FilterMode filter_mode)
        { m_texture_desc.filter_mode_min = filter_mode; }

    HYP_NODISCARD HYP_FORCE_INLINE
    FilterMode GetMagFilterMode() const
        { return m_texture_desc.filter_mode_mag; }

    HYP_FORCE_INLINE
    void SetMagFilterMode(FilterMode filter_mode)
        { m_texture_desc.filter_mode_mag = filter_mode; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const Extent3D &GetExtent() const
        { return m_texture_desc.extent; }

    HYP_NODISCARD HYP_FORCE_INLINE
    InternalFormat GetTextureFormat() const
        { return m_texture_desc.format; }

    HYP_FORCE_INLINE
    void SetTextureFormat(InternalFormat format)
        { m_texture_desc.format = format; }

    HYP_NODISCARD HYP_FORCE_INLINE
    ImageType GetType() const
        { return m_texture_desc.type; }

protected:
    ImageFlags  m_flags;
    
private:

    ImagePlatformImpl<PLATFORM>                 m_platform_impl;

    TextureDesc                                 m_texture_desc;
    RC<StreamedTextureData>                     m_streamed_data;

    bool                                        m_is_blended;
    bool                                        m_is_rw_texture;
    bool                                        m_is_attachment_texture;

    SizeType                                    m_size;
    SizeType                                    m_bpp; // bytes per pixel
};

template <PlatformType PLATFORM>
class StorageImage : public Image<PLATFORM>
{
public:
    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : Image<PLATFORM>(
            TextureDesc
            {
                type,
                format,
                extent,
                min_filter_mode,
                mag_filter_mode,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1, 1
            }
        )
    {
        Image<PLATFORM>::SetIsRWTexture(true);
    }

    StorageImage()
        : StorageImage(
            Extent3D { 1, 1, 1 },
            InternalFormat::RGBA16F,
            ImageType::TEXTURE_TYPE_2D,
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST
        )
    {
    }

    StorageImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type
    ) : StorageImage(
            extent,
            format,
            type,
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST
        )
    {
    }

    StorageImage(const StorageImage &other)             = delete;
    StorageImage &operator=(const StorageImage &other)  = delete;

    StorageImage(StorageImage &&other) noexcept
        : Image<PLATFORM>(std::move(other))
    {
    }

    StorageImage &operator=(StorageImage &&other) noexcept
    {
        Image<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~StorageImage()                                     = default;
};


template <PlatformType PLATFORM>
class SampledImage : public Image<PLATFORM>
{
public:
    SampledImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : Image<PLATFORM>(TextureDesc {
            type,
            format,
            extent,
            min_filter_mode,
            mag_filter_mode,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1, 1
        })   
    {
    }

    SampledImage(const SampledImage &other)             = delete;
    SampledImage &operator=(const SampledImage &other)  = delete;

    SampledImage(SampledImage &&other) noexcept
        : Image<PLATFORM>(std::move(other))
    {
    }

    SampledImage &operator=(SampledImage &&other) noexcept
    {
        Image<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~SampledImage()                                     = default;
};

template <PlatformType PLATFORM>
class SampledImage2D : public SampledImage<PLATFORM>
{
public:
    SampledImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : SampledImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }

    SampledImage2D(const SampledImage2D &other)             = delete;
    SampledImage2D &operator=(const SampledImage2D &other)  = delete;

    SampledImage2D(SampledImage2D &&other) noexcept
        : SampledImage<PLATFORM>(std::move(other))
    {
    }

    SampledImage2D &operator=(SampledImage2D &&other) noexcept
    {
        SampledImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~SampledImage2D()                                       = default;
};

template <PlatformType PLATFORM>
class SampledImage3D : public SampledImage<PLATFORM>
{
public:
    SampledImage3D(
        Extent3D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : SampledImage<PLATFORM>(
            extent,
            format,
            ImageType::TEXTURE_TYPE_3D,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }

    SampledImage3D(const SampledImage3D &other)             = delete;
    SampledImage3D &operator=(const SampledImage3D &other)  = delete;

    SampledImage3D(SampledImage3D &&other) noexcept
        : SampledImage<PLATFORM>(std::move(other))
    {
    }

    SampledImage3D &operator=(SampledImage3D &&other) noexcept
    {
        SampledImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~SampledImage3D()                                       = default;
};

template <PlatformType PLATFORM>
class SampledImageCube : public SampledImage<PLATFORM>
{
public:
    SampledImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : SampledImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }

    SampledImageCube(const SampledImageCube &other)             = delete;
    SampledImageCube &operator=(const SampledImageCube &other)  = delete;

    SampledImageCube(SampledImageCube &&other) noexcept
        : SampledImage<PLATFORM>(std::move(other))
    {
    }

    SampledImageCube &operator=(SampledImageCube &&other) noexcept
    {
        SampledImage<PLATFORM>::operator=(std::move(other));

        return *this;
    }

    ~SampledImageCube()                                         = default;
};

template <PlatformType PLATFORM>
class FramebufferImage : public Image<PLATFORM>
{
public:
    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type
    ) : Image<PLATFORM>(
            TextureDesc
            {
                type,
                format,
                extent,
                FilterMode::TEXTURE_FILTER_NEAREST,
                FilterMode::TEXTURE_FILTER_NEAREST,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1, 1
            }
        )
    {
        Image<PLATFORM>::SetIsAttachmentTexture(true);
    }

    FramebufferImage(
        Extent3D extent,
        InternalFormat format,
        ImageType type,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : Image<PLATFORM>(
            TextureDesc
            {
                ImageType::TEXTURE_TYPE_2D,
                format,
                extent,
                min_filter_mode,
                mag_filter_mode,
                WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
                1, 1
            }
        )
    {
        Image<PLATFORM>::SetIsAttachmentTexture(true);
    }
};

template <PlatformType PLATFORM>
class FramebufferImage2D : public FramebufferImage<PLATFORM>
{
public:
    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D
        )
    {
    }

    FramebufferImage2D(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_2D,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }
};

template <PlatformType PLATFORM>
class FramebufferImageCube : public FramebufferImage<PLATFORM>
{
public:
    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP
        )
    {
    }

    FramebufferImageCube(
        Extent2D extent,
        InternalFormat format,
        FilterMode min_filter_mode,
        FilterMode mag_filter_mode
    ) : FramebufferImage<PLATFORM>(
            Extent3D(extent),
            format,
            ImageType::TEXTURE_TYPE_CUBEMAP,
            min_filter_mode,
            mag_filter_mode
        )
    {
    }
};

} // namespace platform

} // namespace hyperion::renderer

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImage.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Image = platform::Image<Platform::CURRENT>;
using StorageImage = platform::StorageImage<Platform::CURRENT>;
using SampledImage = platform::SampledImage<Platform::CURRENT>;
using SampledImage2D = platform::SampledImage2D<Platform::CURRENT>;
using SampledImage3D = platform::SampledImage3D<Platform::CURRENT>;
using SampledImageCube = platform::SampledImageCube<Platform::CURRENT>;
using FramebufferImage = platform::FramebufferImage<Platform::CURRENT>;
using FramebufferImage2D = platform::FramebufferImage2D<Platform::CURRENT>;
using FramebufferImageCube = platform::FramebufferImageCube<Platform::CURRENT>;

} // namespace renderer

} // namespace hyperion

#endif