/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_GBUFFER_HPP
#define HYPERION_GBUFFER_HPP

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/utilities/Variant.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/RenderBucket.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;

using renderer::DefaultImageFormatType;

using GBufferFormat = Variant<DefaultImageFormatType, InternalFormat, Array<InternalFormat>>;

enum GBufferResourceName : uint32
{
    GBUFFER_RESOURCE_ALBEDO = 0,
    GBUFFER_RESOURCE_NORMALS = 1,
    GBUFFER_RESOURCE_MATERIAL = 2,
    GBUFFER_RESOURCE_TANGENTS = 3,
    GBUFFER_RESOURCE_VELOCITY = 4,
    GBUFFER_RESOURCE_MASK = 5,
    GBUFFER_RESOURCE_WS_NORMALS = 6,
    GBUFFER_RESOURCE_DEPTH = 7,

    GBUFFER_RESOURCE_MAX
};

static_assert(uint32(GBufferResourceName::GBUFFER_RESOURCE_MAX) == num_gbuffer_textures, "GBufferResourceName enum does not match num_gbuffer_textures");

struct GBufferResource
{
    GBufferFormat format;
};

class GBuffer
{
public:
    static const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> gbuffer_resources;

    class GBufferBucket
    {
        friend class GBuffer;

        GBuffer* m_gbuffer;
        Bucket m_bucket;
        FramebufferRef m_framebuffer;

    public:
        GBufferBucket();
        GBufferBucket(const GBufferBucket& other) = delete;
        GBufferBucket& operator=(const GBufferBucket& other) = delete;
        GBufferBucket(GBufferBucket&& other) noexcept = delete;
        GBufferBucket& operator=(GBufferBucket&& other) noexcept = delete;
        ~GBufferBucket();

        HYP_FORCE_INLINE GBuffer* GetGBuffer() const
        {
            return m_gbuffer;
        }

        HYP_FORCE_INLINE void SetGBuffer(GBuffer* gbuffer)
        {
            m_gbuffer = gbuffer;
        }

        HYP_FORCE_INLINE Bucket GetBucket() const
        {
            return m_bucket;
        }

        HYP_FORCE_INLINE void SetBucket(Bucket bucket)
        {
            m_bucket = bucket;
        }

        HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
        {
            return m_framebuffer;
        }

        HYP_FORCE_INLINE void SetFramebuffer(const FramebufferRef& framebuffer)
        {
            m_framebuffer = framebuffer;
        }

        AttachmentBase* GetGBufferAttachment(GBufferResourceName resource_name) const;
    };

    GBuffer(Vec2u extent);
    GBuffer(const GBuffer& other) = delete;
    GBuffer& operator=(const GBuffer& other) = delete;
    GBuffer(GBuffer&& other) noexcept = delete;
    GBuffer& operator=(GBuffer&& other) noexcept = delete;
    ~GBuffer() = default;

    HYP_FORCE_INLINE GBufferBucket& GetBucket(Bucket bucket)
    {
        return m_buckets[int(bucket) - 1];
    }

    HYP_FORCE_INLINE const GBufferBucket& GetBucket(Bucket bucket) const
    {
        return m_buckets[int(bucket) - 1];
    }

    HYP_FORCE_INLINE const Vec2u& GetExtent() const
    {
        return m_extent;
    }

    void Create();
    void Destroy();

    void Resize(Vec2u resolution);

    Delegate<void, Vec2u> OnGBufferResolutionChanged;

private:
    FramebufferRef CreateFramebuffer(const FramebufferRef& opaque_framebuffer, Vec2u resolution, Bucket bucket);

    FixedArray<GBufferBucket, uint32(Bucket::BUCKET_MAX) - 1> m_buckets;
    Array<FramebufferRef> m_framebuffers;

    Vec2u m_extent;
};

} // namespace hyperion

#endif