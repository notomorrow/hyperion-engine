/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEXTURE_HPP
#define HYPERION_TEXTURE_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Pimpl.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/threading/Mutex.hpp>

#include <core/utilities/Pair.hpp>

#include <core/object/HypObject.hpp>

#include <streaming/StreamedData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>

#include <scene/VisibilityState.hpp>

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <Types.hpp>

namespace hyperion {

class StreamedTextureData;
class RenderTexture;

/*! \brief Game-thread side representation of a texture in the engine.
 *  \details This class is used to manage texture data, including streamed textures, and provides methods for sampling and manipulating texture data at runtime. */
HYP_CLASS()
class HYP_API Texture final : public HypObject<Texture>
{
    HYP_OBJECT_BODY(Texture);

public:
    static const FixedArray<Pair<Vec3f, Vec3f>, 6> cubemapDirections;

    Texture();

    Texture(const TextureDesc& textureDesc);
    Texture(const TextureData& textureData);

    Texture(const RC<StreamedTextureData>& streamedTextureData);

    Texture(const Texture& other) = delete;
    Texture& operator=(const Texture& other) = delete;

    Texture(Texture&& other) noexcept = delete;
    Texture& operator=(Texture&& other) noexcept = delete;

    ~Texture();

    HYP_FORCE_INLINE RenderTexture& GetRenderResource() const
    {
        return *m_renderResource;
    }

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD(Property = "Name", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetName(Name name)
    {
        m_name = name;
    }

    HYP_FORCE_INLINE const RC<StreamedTextureData>& GetStreamedTextureData() const
    {
        return m_streamedTextureData;
    }

    /*! \brief Set streamed data for the image. If the image has already been created, no updates will occur. */
    void SetStreamedTextureData(const RC<StreamedTextureData>& streamedTextureData);

    HYP_FORCE_INLINE const TextureDesc& GetTextureDesc() const
    {
        return m_textureDesc;
    }

    void SetTextureDesc(const TextureDesc& textureDesc);

    HYP_FORCE_INLINE TextureType GetType() const
    {
        return m_textureDesc.type;
    }

    HYP_FORCE_INLINE uint32 NumFaces() const
    {
        return m_textureDesc.NumFaces();
    }

    HYP_FORCE_INLINE bool IsTextureCube() const
    {
        return m_textureDesc.IsTextureCube();
    }

    HYP_FORCE_INLINE bool IsPanorama() const
    {
        return m_textureDesc.IsPanorama();
    }

    HYP_FORCE_INLINE const Vec3u& GetExtent() const
    {
        return m_textureDesc.extent;
    }

    HYP_FORCE_INLINE TextureFormat GetFormat() const
    {
        return m_textureDesc.format;
    }

    HYP_FORCE_INLINE HYP_DEPRECATED TextureFilterMode GetFilterMode() const
    {
        return m_textureDesc.filterModeMin;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMinFilterMode() const
    {
        return m_textureDesc.filterModeMin;
    }

    HYP_FORCE_INLINE TextureFilterMode GetMagFilterMode() const
    {
        return m_textureDesc.filterModeMag;
    }

    HYP_FORCE_INLINE bool HasMipmaps() const
    {
        return m_textureDesc.HasMipmaps();
    }

    HYP_FORCE_INLINE TextureWrapMode GetWrapMode() const
    {
        return m_textureDesc.wrapMode;
    }

    void GenerateMipmaps();

    /*! \brief Copies the texture data to the CPU. Waits (blocking) for the render thread to execute the task.
     *  \note While this method is usable from any thread, it is not thread-safe as it modifies the streamed texture data. of the image.
     *  Ensure that the image is not being used in other threads before calling this method.
     *
     *  The texture data will be copied to the CPU and the image will have its StreamedTextureData recreated. */
    void Readback();

    void Resize(const Vec3u& extent);

    Vec4f Sample(Vec3f uvw, uint32 faceIndex);
    Vec4f Sample2D(Vec2f uv);
    Vec4f SampleCube(Vec3f direction);

    /*! \brief Set the Texture to have its render resource always enabled.
     *  \note Init() must be called before this method. */
    void SetPersistentRenderResourceEnabled(bool enabled);

protected:
    void Init() override;

    /*! \brief Readback() implementation, without locking mutex. */
    void Readback_Internal();

    Name m_name;

    RenderTexture* m_renderResource;
    ResourceHandle m_renderPersistent;

    TextureDesc m_textureDesc;

    // MUST BE BEFORE m_streamedTextureData, needs to get constructed first to use as out parameter to construct StreamedTextureData.
    mutable ResourceHandle m_streamedTextureDataResourceHandle;
    RC<StreamedTextureData> m_streamedTextureData;

    Mutex m_readbackMutex;
};

} // namespace hyperion

#endif