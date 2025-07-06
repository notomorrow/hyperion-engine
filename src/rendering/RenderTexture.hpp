/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <core/containers/Bitset.hpp>

#include <core/utilities/DataMutationState.hpp>

#include <core/object/HypObject.hpp>

#include <core/threading/Task.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Texture;

class RenderTexture final : public RenderResourceBase
{
public:
    RenderTexture(Texture* texture);
    virtual ~RenderTexture() override;

    HYP_FORCE_INLINE Texture* GetTexture() const
    {
        return m_texture;
    }

    HYP_FORCE_INLINE const ImageRef& GetImage() const
    {
        return m_image;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetImageView() const
    {
        return m_imageView;
    }

    /*! \brief Enqueues a render command to generate mipmaps for the texture and waits for it to finish.
     *  Thread-safe, blocking function. Use sparingly. */
    void RenderMipmaps();

    void EnqueueReadback(Proc<void(TResult<ByteBuffer>&&)>&& onComplete);
    RendererResult Readback(ByteBuffer& outByteBuffer);

    void Resize(const Vec3u& extent);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GpuBufferHolderBase* GetGpuBufferHolder() const override;

private:
    void UpdateBufferData();

    Texture* m_texture;

    ImageRef m_image;
    ImageViewRef m_imageView;
};

} // namespace hyperion
