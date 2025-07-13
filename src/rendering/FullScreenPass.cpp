/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderSwapchain.hpp> // temp
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderFramebuffer.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <core/math/MathUtil.hpp>

#include <EngineGlobals.hpp>
#include <Types.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

struct MergeHalfResTexturesUniforms
{
    Vec2u dimensions;
};

#pragma region Render commands

struct RENDER_COMMAND(RecreateFullScreenPassFramebuffer)
    : RenderCommand
{
    FullScreenPass& fullScreenPass;
    Vec2u newSize;

    RENDER_COMMAND(RecreateFullScreenPassFramebuffer)(FullScreenPass& fullScreenPass, Vec2u newSize)
        : fullScreenPass(fullScreenPass),
          newSize(newSize)
    {
    }

    virtual ~RENDER_COMMAND(RecreateFullScreenPassFramebuffer)() override = default;

    virtual RendererResult operator()() override
    {
        if (fullScreenPass.m_isInitialized)
        {
            fullScreenPass.Resize_Internal(newSize);
        }
        else
        {
            fullScreenPass.m_extent = newSize;
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

FullScreenPass::FullScreenPass(TextureFormat imageFormat, GBuffer* gbuffer)
    : FullScreenPass(nullptr, imageFormat, Vec2u::Zero(), gbuffer)
{
}

FullScreenPass::FullScreenPass(TextureFormat imageFormat, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(nullptr, imageFormat, extent, gbuffer)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer)
    : FullScreenPass(
          shader,
          descriptorTable,
          FramebufferRef::Null(),
          imageFormat,
          extent,
          gbuffer)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer)
    : FullScreenPass(
          shader,
          DescriptorTableRef::Null(),
          FramebufferRef::Null(),
          imageFormat,
          extent,
          gbuffer)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    const FramebufferRef& framebuffer,
    TextureFormat imageFormat,
    Vec2u extent,
    GBuffer* gbuffer)
    : m_shader(shader),
      m_framebuffer(framebuffer),
      m_imageFormat(imageFormat),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_blendFunction(BlendFunction::None()),
      m_isInitialized(false),
      m_isFirstFrame(true)
{
    if (descriptorTable.IsValid())
    {
        m_descriptorTable.Set(descriptorTable);
    }
}

FullScreenPass::~FullScreenPass()
{
    m_fullScreenQuad.Reset();

    SafeRelease(std::move(m_framebuffer));
    SafeRelease(std::move(m_graphicsPipeline));

    if (m_isInitialized)
    {
        // Prevent dangling reference from render commands
        HYP_SYNC_RENDER();
    }
}

const ImageViewRef& FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending())
    {
        Assert(m_temporalBlending != nullptr);

        return m_temporalBlending->GetResultTexture()->GetRenderResource().GetImageView();
    }

    if (ShouldRenderHalfRes())
    {
        return m_mergeHalfResTexturesPass->GetFinalImageView();
    }

    AttachmentBase* colorAttachment = GetAttachment(0);

    if (!colorAttachment)
    {
        return ImageViewRef::Null();
    }

    return colorAttachment->GetImageView();
}

const ImageViewRef& FullScreenPass::GetPreviousFrameColorImageView() const
{
    // If we're rendering at half res, we use the same image we render to but at an offset.
    if (ShouldRenderHalfRes())
    {
        AttachmentBase* colorAttachment = GetAttachment(0);

        if (!colorAttachment)
        {
            return ImageViewRef::Null();
        }

        return colorAttachment->GetImageView();
    }

    if (m_previousTexture.IsValid())
    {
        return m_previousTexture->GetRenderResource().GetImageView();
    }

    return ImageViewRef::Null();
}

void FullScreenPass::Create()
{
    HYP_SCOPE;

    Assert(!m_isInitialized);

    Assert(
        m_imageFormat != TF_NONE,
        "Image format must be set before creating the full screen pass");

    CreateQuad();
    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();
    CreatePipeline();

    m_isInitialized = true;
}

void FullScreenPass::SetShader(const ShaderRef& shader)
{
    if (m_shader == shader)
    {
        return;
    }

    m_shader = shader;
}

AttachmentBase* FullScreenPass::GetAttachment(uint32 attachmentIndex) const
{
    Assert(m_framebuffer.IsValid());

    return m_framebuffer->GetAttachment(attachmentIndex);
}

void FullScreenPass::SetBlendFunction(const BlendFunction& blendFunction)
{
    m_blendFunction = blendFunction;
}

void FullScreenPass::Resize(Vec2u newSize)
{
    PUSH_RENDER_COMMAND(RecreateFullScreenPassFramebuffer, *this, newSize);
}

void FullScreenPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_extent == newSize)
    {
        return;
    }

    AssertDebug(newSize.Volume() != 0, "Cannot resize FullScreenPass to zero size!");

    newSize = MathUtil::Max(newSize, Vec2u::One());
    m_extent = newSize;

    if (!m_framebuffer.IsValid())
    {
        // Not created yet; skip
        return;
    }

    SafeRelease(std::move(m_graphicsPipeline));
    SafeRelease(std::move(m_framebuffer));

    m_temporalBlending.Reset();

    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();
    CreatePipeline();
}

void FullScreenPass::CreateQuad()
{
    HYP_SCOPE;

    m_fullScreenQuad = MeshBuilder::Quad();
    InitObject(m_fullScreenQuad);

    // Allow Render() to be called directly without a RenderGroup
    m_fullScreenQuad->SetPersistentRenderResourceEnabled(true);
}

void FullScreenPass::CreateFramebuffer()
{
    HYP_SCOPE;

    if (m_framebuffer.IsValid())
    {
        Assert(m_framebuffer->GetExtent() == m_extent);
        DeferCreate(m_framebuffer);

        return;
    }

    if (m_extent.x * m_extent.y == 0)
    {
        // Assert(m_gbuffer != nullptr);
        // m_extent = m_gbuffer->GetExtent();
        // TEMP HACK
        m_extent = g_renderBackend->GetSwapchain()->GetExtent();
    }

    Vec2u framebufferExtent = m_extent;

    if (ShouldRenderHalfRes())
    {
        constexpr double resolutionScale = 0.5;

        const uint32 numPixels = framebufferExtent.x * framebufferExtent.y;
        const int numPixelsScaled = MathUtil::Ceil(numPixels * resolutionScale);

        const Vec2i reshapedExtent = MathUtil::ReshapeExtent(Vec2i { numPixelsScaled, 1 });

        // double the width as we swap between the two halves when rendering (checkerboarded)
        framebufferExtent = Vec2u { uint32(reshapedExtent.x * 2), uint32(reshapedExtent.y) };
    }

    m_framebuffer = g_renderBackend->MakeFramebuffer(framebufferExtent);

    TextureDesc textureDesc;
    textureDesc.type = TT_TEX2D;
    textureDesc.format = m_imageFormat;
    textureDesc.extent = Vec3u { framebufferExtent, 1 };
    textureDesc.filterModeMin = TFM_NEAREST;
    textureDesc.filterModeMag = TFM_NEAREST;
    textureDesc.wrapMode = TWM_CLAMP_TO_EDGE;
    textureDesc.imageUsage = IU_ATTACHMENT | IU_SAMPLED;

    ImageRef attachmentImage = g_renderBackend->MakeImage(textureDesc);
    DeferCreate(attachmentImage);

    AttachmentRef attachment = m_framebuffer->AddAttachment(
        0,
        attachmentImage,
        ShouldRenderHalfRes() ? LoadOperation::LOAD : LoadOperation::CLEAR,
        StoreOperation::STORE);

    if (m_blendFunction != BlendFunction::None())
    {
        attachment->SetAllowBlending(true);
    }

    DeferCreate(attachment);

    DeferCreate(m_framebuffer);
}

void FullScreenPass::CreatePipeline()
{
    HYP_SCOPE;

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertexAttributes = staticMeshVertexAttributes },
        MaterialAttributes {
            .fillMode = FM_FILL,
            .blendFunction = m_blendFunction,
            .flags = MAF_NONE }));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;

    m_graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        m_descriptorTable.GetOr(DescriptorTableRef::Null()),
        { &m_framebuffer, 1 },
        renderableAttributes);
}

void FullScreenPass::CreateTemporalBlending()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending())
    {
        return;
    }

    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_extent,
        TF_RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        ShouldRenderHalfRes()
            ? m_mergeHalfResTexturesPass->GetFinalImageView()
            : GetAttachment(0)->GetImageView(),
        m_gbuffer);

    m_temporalBlending->Create();
}

void FullScreenPass::CreatePreviousTexture()
{
    // Create previous image
    m_previousTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE });

    InitObject(m_previousTexture);

    m_previousTexture->SetPersistentRenderResourceEnabled(true);
}

void FullScreenPass::CreateRenderTextureToScreenPass()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending())
    {
        return;
    }

    if (!ShouldRenderHalfRes())
    {
        CreatePreviousTexture();
    }

    // Create render texture to screen pass.
    // this is used to render the previous frame's result to the screen,
    // so we can blend it with the current frame's result (checkerboarded)

    ShaderProperties shaderProperties;

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Set("HALFRES");
    }

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"), shaderProperties);
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), GetPreviousFrameColorImageView());
    }

    DeferCreate(descriptorTable);

    m_renderTextureToScreenPass = MakeUnique<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        nullptr);

    m_renderTextureToScreenPass->Create();
}

void FullScreenPass::CreateMergeHalfResTexturesPass()
{
    HYP_SCOPE;

    if (!ShouldRenderHalfRes())
    {
        return;
    }

    GpuBufferRef mergeHalfResTexturesUniformBuffer;

    { // Create uniform buffer
        MergeHalfResTexturesUniforms uniforms;
        uniforms.dimensions = m_extent;

        mergeHalfResTexturesUniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
        HYPERION_ASSERT_RESULT(mergeHalfResTexturesUniformBuffer->Create());
        mergeHalfResTexturesUniformBuffer->Copy(sizeof(uniforms), &uniforms);
    }

    ShaderRef mergeHalfResTexturesShader = g_shaderManager->GetOrCreate(NAME("MergeHalfResTextures"));
    Assert(mergeHalfResTexturesShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = mergeHalfResTexturesShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("MergeHalfResTexturesDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InTexture"), GetAttachment(0)->GetImageView());
        descriptorSet->SetElement(NAME("UniformBuffer"), mergeHalfResTexturesUniformBuffer);
    }

    DeferCreate(descriptorTable);

    m_mergeHalfResTexturesPass = MakeUnique<FullScreenPass>(
        mergeHalfResTexturesShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        nullptr);

    m_mergeHalfResTexturesPass->Create();
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::RenderPreviousTextureToScreen(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_renderTextureToScreenPass != nullptr);

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (renderSetup.world->GetBufferData().frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        // render previous frame's result to screen
        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_renderTextureToScreenPass->GetGraphicsPipeline(),
            viewportOffset,
            viewportExtent);
    }
    else
    {
        // render previous frame's result to screen
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_renderTextureToScreenPass->GetGraphicsPipeline());
    }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable(),
        m_renderTextureToScreenPass->GetGraphicsPipeline(),
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->GetCommandList().Add<BindDescriptorSet>(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            m_renderTextureToScreenPass->GetGraphicsPipeline(),
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);
    }

    frame->GetCommandList().Add<BindVertexBuffer>(m_fullScreenQuad->GetVertexBuffer());
    frame->GetCommandList().Add<BindIndexBuffer>(m_fullScreenQuad->GetIndexBuffer());
    frame->GetCommandList().Add<DrawIndexed>(m_fullScreenQuad->NumIndices());
}

void FullScreenPass::CopyResultToPreviousTexture(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_previousTexture.IsValid());

    const ImageRef& srcImage = m_framebuffer->GetAttachment(0)->GetImage();
    const ImageRef& dstImage = m_previousTexture->GetRenderResource().GetImage();

    frame->GetCommandList().Add<InsertBarrier>(srcImage, RS_COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(dstImage, RS_COPY_DST);

    frame->GetCommandList().Add<Blit>(srcImage, dstImage);

    frame->GetCommandList().Add<InsertBarrier>(srcImage, RS_SHADER_RESOURCE);
    frame->GetCommandList().Add<InsertBarrier>(dstImage, RS_SHADER_RESOURCE);
}

void FullScreenPass::MergeHalfResTextures(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_mergeHalfResTexturesPass != nullptr);

    m_mergeHalfResTexturesPass->Render(frame, renderSetup);
}

void FullScreenPass::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frameIndex);

    RenderToFramebuffer(frame, renderSetup, m_framebuffer);

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frameIndex);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }
}

void FullScreenPass::RenderToFramebuffer(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, renderSetup);
    }

    m_graphicsPipeline->SetPushConstants(m_pushConstantData.Data(), m_pushConstantData.Size());

    if (ShouldRenderHalfRes())
    {
        Assert(framebuffer != nullptr, "Framebuffer must be set before rendering to it, if rendering at half res");

        const Vec2i viewportOffset = (Vec2i(framebuffer->GetExtent().x, 0) / 2) * (renderSetup.world->GetBufferData().frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(framebuffer->GetExtent().x / 2, framebuffer->GetExtent().y);

        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_graphicsPipeline,
            viewportOffset,
            viewportExtent);
    }
    else
    {
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_graphicsPipeline);
    }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_graphicsPipeline->GetDescriptorTable(),
        m_graphicsPipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) } } } },
        frame->GetFrameIndex());

    const uint32 viewDescriptorSetIndex = m_graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->GetCommandList().Add<BindDescriptorSet>(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            m_graphicsPipeline,
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);
    }

    frame->GetCommandList().Add<BindVertexBuffer>(m_fullScreenQuad->GetVertexBuffer());
    frame->GetCommandList().Add<BindIndexBuffer>(m_fullScreenQuad->GetIndexBuffer());
    frame->GetCommandList().Add<DrawIndexed>(m_fullScreenQuad->NumIndices());

    m_isFirstFrame = false;
}

void FullScreenPass::Begin(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frameIndex);

    if (ShouldRenderHalfRes())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (renderSetup.world->GetBufferData().frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_graphicsPipeline,
            viewportOffset,
            viewportExtent);
    }
    else
    {
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_graphicsPipeline);
    }
}

void FullScreenPass::End(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    // render previous frame's result to screen
    if (!m_isFirstFrame && m_renderTextureToScreenPass != nullptr)
    {
        RenderPreviousTextureToScreen(frame, renderSetup);
    }

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frameIndex);

    if (ShouldRenderHalfRes())
    {
        MergeHalfResTextures(frame, renderSetup);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderHalfRes())
        {
            CopyResultToPreviousTexture(frame, renderSetup);
        }

        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isFirstFrame = false;
}

} // namespace hyperion
