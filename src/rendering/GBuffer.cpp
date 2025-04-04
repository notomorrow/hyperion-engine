/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/GBuffer.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/backend/RendererFeatures.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region GBuffer

const FixedArray<GBufferResource, GBUFFER_RESOURCE_MAX> GBuffer::gbuffer_resources = {
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_COLOR) },    // color
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) },  // normal
    GBufferResource { GBufferFormat(InternalFormat::RGBA8) },           // material
    GBufferResource {                                                   // lightmap
        GBufferFormat(Array<InternalFormat> {
            InternalFormat::R11G11B10F,
            InternalFormat::RGBA16F
        })
    },
    GBufferResource { GBufferFormat(InternalFormat::RG16F) },           // velocity
    GBufferResource {                                                   // objects mask
        GBufferFormat(Array<InternalFormat> {
            InternalFormat::R16
        })
    },
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_NORMALS) },  // world-space normals (untextured)
    GBufferResource { GBufferFormat(TEXTURE_FORMAT_DEFAULT_DEPTH) }     // depth
};

static void AddOwnedAttachment(uint32 binding, InternalFormat format, const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
    
    const Vec2u &extent = framebuffer->GetExtent();
    AssertThrow(extent.Volume() != 0);

    ImageRef attachment_image = MakeRenderObject<Image>(renderer::FramebufferImage2D(extent, format));
    HYPERION_ASSERT_RESULT(attachment_image->Create(g_engine->GetGPUInstance()->GetDevice()));

    AttachmentRef attachment = MakeRenderObject<Attachment>(
        attachment_image,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );
    
    attachment->SetBinding(binding);

    // if (!attachment_image->IsDepthStencil()) {
    //     attachment->SetAllowBlending(true);
    // }

    HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));

    framebuffer->AddAttachment(std::move(attachment));
}

static void AddSharedAttachment(uint32 binding, const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const FramebufferRef &opaque_framebuffer = g_engine->GetDeferredRenderer()->GetGBuffer()->GetBucket(BUCKET_OPAQUE).GetFramebuffer();
    AssertThrowMsg(opaque_framebuffer.IsValid(), "GBuffer framebuffers added in wrong order");

    const AttachmentRef &parent_attachment = opaque_framebuffer->GetAttachment(binding);
    AssertThrow(parent_attachment.IsValid());
    
    AttachmentRef attachment = MakeRenderObject<Attachment>(
        parent_attachment->GetImage(),
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::LOAD,
        renderer::StoreOperation::STORE
    );

    attachment->SetBinding(binding);
    attachment->SetAllowBlending(false);
    HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));

    framebuffer->AddAttachment(attachment);
}

static InternalFormat GetImageFormat(GBufferResourceName resource)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    InternalFormat color_format = InternalFormat::NONE;

    if (const InternalFormat *format = GBuffer::gbuffer_resources[resource].format.TryGet<InternalFormat>()) {
        color_format = *format;
    } else if (const TextureFormatDefault *default_format = GBuffer::gbuffer_resources[resource].format.TryGet<TextureFormatDefault>()) {
        color_format = g_engine->GetDefaultFormat(*default_format);   
    } else if (const Array<InternalFormat> *default_formats = GBuffer::gbuffer_resources[resource].format.TryGet<Array<InternalFormat>>()) {
        for (const InternalFormat format : *default_formats) {
            if (g_engine->GetGPUDevice()->GetFeatures().IsSupportedFormat(format, renderer::ImageSupportType::SRV)) {
                color_format = format;

                break;
            }
        }
    }

    AssertThrowMsg(color_format != InternalFormat::NONE, "Invalid value set for gbuffer image format");

    return color_format;
}

Vec2u GBuffer::RescaleResolution(Vec2u resolution)
{
    if (const sys::ApplicationWindow *window = g_engine->GetAppContext()->GetMainWindow()) {
        if (window->IsHighDPI()) {
            // if the window is high DPI like retina on mac, we need to scale it down
            resolution /= 2;
        }
    }

    return resolution;
}

GBuffer::GBuffer()
    : m_resolution(0, 0)
{
    for (SizeType i = 0; i < m_buckets.Size(); i++) {
        m_buckets[i].SetBucket(Bucket(i));
    }
}

void GBuffer::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_resolution = RescaleResolution(g_engine->GetGPUInstance()->GetSwapchain()->extent);

    for (auto &bucket : m_buckets) {
        bucket.CreateFramebuffer(m_resolution);
    }
}

void GBuffer::Destroy()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    for (auto &bucket : m_buckets) {
        bucket.Destroy();
    }
}

void GBuffer::Resize(Vec2u resolution)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    resolution = RescaleResolution(resolution);

    for (GBufferBucket &bucket : m_buckets) {
        bucket.Resize(resolution);
    }

    m_resolution = resolution;

    OnGBufferResolutionChanged(resolution);
}

#pragma endregion GBuffer

#pragma region GBufferBucket

GBuffer::GBufferBucket::GBufferBucket()
{
}

GBuffer::GBufferBucket::~GBufferBucket()
{
}

const AttachmentRef &GBuffer::GBufferBucket::GetGBufferAttachment(GBufferResourceName resource_name) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(framebuffer != nullptr);
    AssertThrow(uint32(resource_name) < uint32(GBUFFER_RESOURCE_MAX));

    return framebuffer->GetAttachment(uint32(resource_name));
}

void GBuffer::GBufferBucket::CreateFramebuffer(Vec2u resolution)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    renderer::RenderPassMode mode = renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER;

    if (bucket == BUCKET_SWAPCHAIN /*|| bucket == BUCKET_UI*/) {
        mode = renderer::RenderPassMode::RENDER_PASS_INLINE;
    }

    Vec2u framebuffer_extent = resolution;

    // double resolution for UI
    if (bucket == BUCKET_UI) {
        framebuffer_extent *= 2;
    }

    framebuffer = MakeRenderObject<Framebuffer>(framebuffer_extent, renderer::RenderPassStage::SHADER, mode);

    const InternalFormat color_format = GetImageFormat(GBUFFER_RESOURCE_ALBEDO);

    if (bucket == BUCKET_UI) {
        // UI is floating point to give more room for using HDR textures on objects.
        AddOwnedAttachment(0, InternalFormat::RGBA16F, framebuffer);

        // Needed for stencil
        AddOwnedAttachment(1, InternalFormat::DEPTH_32F, framebuffer);
    } else if (BucketIsRenderable(bucket)) {
        // add gbuffer attachments
        // color attachment is unique for all buckets
        AddOwnedAttachment(0, color_format, framebuffer);

        // opaque creates the main non-color gbuffer attachments,
        // which will be shared with other renderable buckets
        if (bucket == BUCKET_OPAQUE) {
            for (uint32 i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                const InternalFormat format = GetImageFormat(GBufferResourceName(i));

                AddOwnedAttachment(i, format, framebuffer);
            }
        } else {
            // add the attachments shared with opaque bucket
            for (uint32 i = 1; i < GBUFFER_RESOURCE_MAX; i++) {
                AddSharedAttachment(i, framebuffer);
            }
        }
    }

    DeferCreate(framebuffer, g_engine->GetGPUDevice());
}

void GBuffer::GBufferBucket::Destroy()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    SafeRelease(std::move(framebuffer));
}

void GBuffer::GBufferBucket::Resize(Vec2u resolution)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    Vec2u framebuffer_extent = resolution;

    // double resolution for UI
    if (bucket == BUCKET_UI) {
        framebuffer_extent *= 2;
    }

    HYPERION_ASSERT_RESULT(framebuffer->Resize(g_engine->GetGPUDevice(), framebuffer_extent));
}

#pragma endregion GBufferBucket

} // namespace hyperion
