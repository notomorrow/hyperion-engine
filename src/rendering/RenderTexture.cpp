/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderImageView.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderQueue.hpp>

#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>

#include <asset/TextureAsset.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture)
    : RenderCommand
{
    WeakHandle<Texture> textureWeak;
    ResourceHandle resourceHandle;
    ResourceState initialState;
    ImageRef image;
    ImageViewRef imageView;

    RENDER_COMMAND(CreateTexture)(
        const WeakHandle<Texture>& textureWeak,
        ResourceHandle&& resourceHandle,
        ResourceState initialState,
        ImageRef image,
        ImageViewRef imageView)
        : textureWeak(textureWeak),
          resourceHandle(std::move(resourceHandle)),
          initialState(initialState),
          image(std::move(image)),
          imageView(std::move(imageView))
    {
        Assert(this->image.IsValid());
        Assert(this->imageView.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateTexture)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<Texture> texture = textureWeak.Lock())
        {
            if (!image->IsCreated())
            {
                HYPERION_BUBBLE_ERRORS(image->Create());

                if (texture->GetAsset().IsValid())
                {
                    Assert(resourceHandle);

                    TextureData* textureData = texture->GetAsset()->GetTextureData();
                    Assert(textureData != nullptr);

                    const TextureDesc& textureDesc = texture->GetAsset()->GetTextureDesc();

                    ByteBuffer const* imageData = &textureData->imageData;
                    LinkedList<ByteBuffer> placeholderBuffers;

                    if (textureDesc != image->GetTextureDesc())
                    {
                        HYP_LOG(Streaming, Warning, "Streamed texture data TextureDesc not equal to Image's TextureDesc!");
                    }

                    if (imageData->Size() != image->GetByteSize())
                    {
                        HYP_LOG(Streaming, Warning, "Streamed texture data buffer size mismatch! Expected: {}, Got: {}",
                            image->GetByteSize(), textureDesc.GetByteSize());

                        // fill some placeholder data with zeros so we don't crash
                        ByteBuffer* placeholderBuffer = &placeholderBuffers.EmplaceBack();
                        placeholderBuffer->SetSize(image->GetByteSize());

                        const TextureFormat nonSrgbFormat = ChangeFormatSrgb(image->GetTextureFormat(), false);

                        switch (texture->GetType())
                        {
                        case TT_TEX2D:
                            switch (nonSrgbFormat)
                            {
                            case TF_R8:
                                FillPlaceholderBuffer_Tex2D<TF_R8>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            case TF_RGBA8:
                                FillPlaceholderBuffer_Tex2D<TF_RGBA8>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            case TF_RGBA16F:
                                FillPlaceholderBuffer_Tex2D<TF_RGBA16F>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            case TF_RGBA32F:
                                FillPlaceholderBuffer_Tex2D<TF_RGBA32F>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            default:
                                // no FillPlaceholderBuffer method defined
                                break;
                            }
                            break;
                        case TT_CUBEMAP:
                            switch (nonSrgbFormat)
                            {
                            case TF_R8:
                                FillPlaceholderBuffer_Cubemap<TF_R8>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            case TF_RGBA8:
                                FillPlaceholderBuffer_Cubemap<TF_RGBA8>(image->GetExtent().GetXY(), *placeholderBuffer);
                                break;
                            default:
                                // no FillPlaceholderBuffer method defined
                                break;
                            }
                            break;
                        default:
                            // no FillPlaceholderBuffer method defined
                            break;
                        }

                        imageData = placeholderBuffer;
                    }

                    GpuBufferRef stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, imageData->Size());
                    HYPERION_BUBBLE_ERRORS(stagingBuffer->Create());
                    stagingBuffer->Copy(imageData->Size(), imageData->Data());

                    HYP_DEFER({ SafeRelease(std::move(stagingBuffer)); });

                    // @FIXME add back ConvertTo32BPP

                    FrameBase* frame = g_renderBackend->GetCurrentFrame();
                    RenderQueue& renderQueue = frame->renderQueue;

                    renderQueue << InsertBarrier(image, RS_COPY_DST);

                    renderQueue << CopyBufferToImage(stagingBuffer, image);

                    if (textureDesc.HasMipmaps())
                    {
                        renderQueue << GenerateMipmaps(image);
                    }

                    renderQueue << InsertBarrier(image, initialState);
                }
                else if (initialState != RS_UNDEFINED)
                {
                    FrameBase* frame = g_renderBackend->GetCurrentFrame();
                    RenderQueue& renderQueue = frame->renderQueue;

                    // Transition to initial state
                    renderQueue << InsertBarrier(image, initialState);
                }
            }

            if (!imageView->IsCreated())
            {
                HYPERION_BUBBLE_ERRORS(imageView->Create());
            }
        }

        return {};
    }
};

struct RENDER_COMMAND(RenderTextureMipmapLevels)
    : RenderCommand
{
    ImageRef m_image;
    ImageViewRef m_imageView;
    Array<ImageViewRef> m_mipImageViews;
    Array<RC<FullScreenPass>> m_passes;

    RENDER_COMMAND(RenderTextureMipmapLevels)(
        ImageRef image,
        ImageViewRef imageView,
        Array<ImageViewRef> mipImageViews,
        Array<RC<FullScreenPass>> passes)
        : m_image(std::move(image)),
          m_imageView(std::move(imageView)),
          m_mipImageViews(std::move(mipImageViews)),
          m_passes(std::move(passes))
    {
        Assert(m_image != nullptr);
        Assert(m_imageView != nullptr);

        Assert(m_passes.Size() == m_mipImageViews.Size());

        for (SizeType index = 0; index < m_mipImageViews.Size(); index++)
        {
            Assert(m_mipImageViews[index] != nullptr);
            Assert(m_passes[index] != nullptr);
        }
    }

    virtual RendererResult operator()() override
    {
        // draw a quad for each level
        FrameBase* frame = g_renderBackend->GetCurrentFrame();
        RenderQueue& renderQueue = frame->renderQueue;

        const Vec3u extent = m_image->GetExtent();

        uint32 mipWidth = extent.x,
               mipHeight = extent.y;

        ImageRef& dstImage = m_image;

        for (uint32 mipLevel = 0; mipLevel < uint32(m_mipImageViews.Size()); mipLevel++)
        {
            RC<FullScreenPass>& pass = m_passes[mipLevel];
            Assert(pass != nullptr);

            const uint32 prevMipWidth = mipWidth,
                         prevMipHeight = mipHeight;

            mipWidth = MathUtil::Max(1u, extent.x >> (mipLevel));
            mipHeight = MathUtil::Max(1u, extent.y >> (mipLevel));

            struct
            {
                Vec4u dimensions;
                Vec4u prevDimensions;
                uint32 mipLevel;
            } pushConstants;

            pushConstants.dimensions = { mipWidth, mipHeight, 0, 0 };
            pushConstants.prevDimensions = { prevMipWidth, prevMipHeight, 0, 0 };
            pushConstants.mipLevel = mipLevel;

            {
                pass->GetGraphicsPipeline()->SetPushConstants(&pushConstants, sizeof(pushConstants));
                pass->Begin(frame, NullRenderSetup());

                renderQueue << BindDescriptorTable(
                    pass->GetGraphicsPipeline()->GetDescriptorTable(),
                    pass->GetGraphicsPipeline(),
                    ArrayMap<Name, ArrayMap<Name, uint32>> {},
                    frame->GetFrameIndex());

                frame->renderQueue << BindVertexBuffer(pass->GetQuadMesh()->GetVertexBuffer());
                frame->renderQueue << BindIndexBuffer(pass->GetQuadMesh()->GetIndexBuffer());
                frame->renderQueue << DrawIndexed(pass->GetQuadMesh()->NumIndices());

                pass->End(frame, NullRenderSetup());
            }

            const ImageRef& srcImage = pass->GetAttachment(0)->GetImage();

            // Blit into mip level
            renderQueue << InsertBarrier(dstImage, RS_COPY_DST, ImageSubResource { .baseMipLevel = mipLevel });
            renderQueue << InsertBarrier(srcImage, RS_COPY_SRC, ImageSubResource { .baseMipLevel = mipLevel });

            renderQueue << BlitRect(
                srcImage,
                dstImage,
                Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
                Rect<uint32> { 0, 0, dstImage->GetExtent().x, dstImage->GetExtent().y });

            renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
            renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
        }

        // all mip levels have been transitioned into this state
        renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);

        return {};
    }
};

#pragma endregion Render commands

#pragma region TextureMipmapRenderer

class TextureMipmapRenderer
{
public:
    TextureMipmapRenderer(ImageRef image, ImageViewRef imageView)
        : m_image(std::move(image)),
          m_imageView(std::move(imageView))
    {
    }

    void Create()
    {
        const uint32 numMipLevels = m_image->NumMipmaps();

        m_mipImageViews.Resize(numMipLevels);
        m_passes.Resize(numMipLevels);

        const Vec3u extent = m_image->GetExtent();

        ShaderRef shader = g_shaderManager->GetOrCreate(
            NAME("GenerateMipmaps"),
            ShaderProperties());

        for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
        {
            const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

            DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

            const uint32 mipWidth = MathUtil::Max(1u, extent.x >> mipLevel);
            const uint32 mipHeight = MathUtil::Max(1u, extent.y >> mipLevel);

            ImageViewRef mipImageView = g_renderBackend->MakeImageView(m_image, mipLevel, 1, 0, m_image->NumFaces());
            DeferCreate(mipImageView);

            const DescriptorSetRef& generateMipmapsDescriptorSet = descriptorTable->GetDescriptorSet(NAME("GenerateMipmapsDescriptorSet"), 0);
            Assert(generateMipmapsDescriptorSet != nullptr);
            generateMipmapsDescriptorSet->SetElement(NAME("InputTexture"), mipLevel == 0 ? m_imageView : m_mipImageViews[mipLevel - 1]);
            DeferCreate(descriptorTable);

            m_mipImageViews[mipLevel] = std::move(mipImageView);

            {
                RC<FullScreenPass> pass = MakeRefCountedPtr<FullScreenPass>(
                    shader,
                    descriptorTable,
                    m_image->GetTextureFormat(),
                    Vec2u { mipWidth, mipHeight },
                    nullptr);

                pass->Create();

                m_passes[mipLevel] = std::move(pass);
            }
        }
    }

    void Destroy()
    {
        m_passes.Clear();

        SafeRelease(std::move(m_image));
        SafeRelease(std::move(m_imageView));
        SafeRelease(std::move(m_mipImageViews));
    }

    void Render()
    {
        PUSH_RENDER_COMMAND(
            RenderTextureMipmapLevels,
            m_image,
            m_imageView,
            m_mipImageViews,
            m_passes);
    }

private:
    ImageRef m_image;
    ImageViewRef m_imageView;
    Array<ImageViewRef> m_mipImageViews;
    Array<RC<FullScreenPass>> m_passes;
};

#pragma endregion TextureMipmapRenderer

#pragma region RenderTexture

RenderTexture::RenderTexture(Texture* texture)
    : m_texture(texture),
      m_image(g_renderBackend->MakeImage(texture->GetTextureDesc())),
      m_imageView(g_renderBackend->MakeImageView(m_image))
{
#ifdef HYP_DEBUG_MODE
    AssertDebug(m_image.IsValid());

    m_image->SetDebugName(NAME_FMT("{} ({})", texture->Id(), texture->GetName()));
#endif
}

RenderTexture::~RenderTexture()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_imageView));
}

void RenderTexture::Initialize_Internal()
{
    HYP_SCOPE;

    PUSH_RENDER_COMMAND(
        CreateTexture,
        m_texture->WeakHandleFromThis(),
        m_texture->GetAsset().IsValid() ? ResourceHandle(*m_texture->GetAsset()->GetResource()) : ResourceHandle(),
        RS_SHADER_RESOURCE,
        m_image,
        m_imageView);
}

void RenderTexture::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderTexture::Update_Internal()
{
    HYP_SCOPE;
}

void RenderTexture::RenderMipmaps()
{
    HYP_SCOPE;

    Execute([this]()
        {
            TextureMipmapRenderer mipmapRenderer(m_image, m_imageView);
            mipmapRenderer.Create();
            mipmapRenderer.Render();
            mipmapRenderer.Destroy();
        });
}

void RenderTexture::EnqueueReadback(Proc<void(TResult<ByteBuffer>&&)>&& onComplete)
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Debug, "Readback called for texture data of size {} bytes", m_image->GetByteSize());

    if (!IsInitialized())
    {
        onComplete(HYP_MAKE_ERROR(Error, "RenderTexture is not initialized, cannot readback texture data"));

        return;
    }

    Execute([this, onComplete = std::move(onComplete)]()
        {
            Threads::AssertOnThread(g_renderThread);

            ByteBuffer byteBuffer;

            HYP_LOG(Rendering, Debug, "Reading back texture data of size {} bytes", m_image->GetByteSize());

            GpuBufferRef gpuBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_image->GetByteSize());
            HYPERION_ASSERT_RESULT(gpuBuffer->Create());
            gpuBuffer->Map();

            UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

            singleTimeCommands->Push([this, &gpuBuffer](RenderQueue& renderQueue)
                {
                    const ResourceState previousResourceState = m_image->GetResourceState();

                    renderQueue << InsertBarrier(m_image, RS_COPY_SRC);
                    renderQueue << InsertBarrier(gpuBuffer, RS_COPY_DST);

                    renderQueue << CopyImageToBuffer(m_image, gpuBuffer);

                    renderQueue << InsertBarrier(gpuBuffer, RS_COPY_SRC);
                    renderQueue << InsertBarrier(m_image, previousResourceState);
                });

            RendererResult result = singleTimeCommands->Execute();

            if (result.HasError())
            {
                HYP_LOG(Rendering, Error, "Failed to readback texture data! {}", result.GetError().GetMessage());

                onComplete(result.GetError());

                return;
            }

            byteBuffer.SetSize(gpuBuffer->Size());
            gpuBuffer->Read(byteBuffer.Size(), byteBuffer.Data());

            gpuBuffer->Destroy();

            HYP_LOG(Rendering, Debug, "Readback texture data of size {} bytes", byteBuffer.Size());

            onComplete(std::move(byteBuffer));
        },
        /* forceOwnerThread */ true);
}

RendererResult RenderTexture::Readback(ByteBuffer& outByteBuffer)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    GpuBufferRef gpuBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, m_image->GetByteSize());
    HYPERION_ASSERT_RESULT(gpuBuffer->Create());
    gpuBuffer->Map();

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

    singleTimeCommands->Push([this, &gpuBuffer](RenderQueue& renderQueue)
        {
            const ResourceState previousResourceState = m_image->GetResourceState();

            renderQueue << InsertBarrier(m_image, RS_COPY_SRC);
            renderQueue << InsertBarrier(gpuBuffer, RS_COPY_DST);

            renderQueue << CopyImageToBuffer(m_image, gpuBuffer);

            renderQueue << InsertBarrier(gpuBuffer, RS_COPY_SRC);
            renderQueue << InsertBarrier(m_image, previousResourceState);
        });

    RendererResult result = singleTimeCommands->Execute();

    if (result.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to readback texture data! {}", result.GetError().GetMessage());

        return result;
    }

    outByteBuffer.SetSize(gpuBuffer->Size());
    gpuBuffer->Read(outByteBuffer.Size(), outByteBuffer.Data());

    gpuBuffer->Destroy();

    return {};
}

void RenderTexture::Resize(const Vec3u& extent)
{
    HYP_SCOPE;

    TextureDesc textureDesc = m_texture->GetTextureDesc();

    Execute([this, extent, textureDesc]()
        {
            // HYPERION_ASSERT_RESULT(m_image->Resize(extent));

            // if (m_imageView->IsCreated())
            // {
            //     m_imageView->Destroy();
            //     HYPERION_ASSERT_RESULT(m_imageView->Create());
            // }

            SafeRelease(std::move(m_image));
            SafeRelease(std::move(m_imageView));

            m_image = g_renderBackend->MakeImage(textureDesc);
            HYPERION_ASSERT_RESULT(m_image->Create());

            m_imageView = g_renderBackend->MakeImageView(m_image);
            HYPERION_ASSERT_RESULT(m_imageView->Create());
        },
        /* forceOwnerThread */ true);
}

GpuBufferHolderBase* RenderTexture::GetGpuBufferHolder() const
{
    return nullptr;
}

#pragma endregion RenderTexture

} // namespace hyperion
