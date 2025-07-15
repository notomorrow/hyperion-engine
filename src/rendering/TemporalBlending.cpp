/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalBlending.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/ShaderManager.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderComputePipeline.hpp>

#include <rendering/Texture.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)
    : RenderCommand
{
    TemporalBlending& temporalBlending;
    Vec2u newSize;

    RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)(TemporalBlending& temporalBlending, const Vec2u& newSize)
        : temporalBlending(temporalBlending),
          newSize(newSize)
    {
    }

    virtual ~RENDER_COMMAND(RecreateTemporalBlendingFramebuffer)() override = default;

    virtual RendererResult operator()() override
    {
        temporalBlending.Resize_Internal(newSize);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const ImageViewRef& inputImageView,
    GBuffer* gbuffer)
    : TemporalBlending(
          extent,
          TF_RGBA8,
          technique,
          feedback,
          inputImageView,
          gbuffer)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TextureFormat imageFormat,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const FramebufferRef& inputFramebuffer,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_imageFormat(imageFormat),
      m_technique(technique),
      m_feedback(feedback),
      m_inputFramebuffer(inputFramebuffer),
      m_gbuffer(gbuffer),
      m_blendingFrameCounter(0),
      m_isInitialized(false)
{
}

TemporalBlending::TemporalBlending(
    const Vec2u& extent,
    TextureFormat imageFormat,
    TemporalBlendTechnique technique,
    TemporalBlendFeedback feedback,
    const ImageViewRef& inputImageView,
    GBuffer* gbuffer)
    : m_extent(extent),
      m_imageFormat(imageFormat),
      m_technique(technique),
      m_feedback(feedback),
      m_inputImageView(inputImageView),
      m_gbuffer(gbuffer),
      m_blendingFrameCounter(0),
      m_isInitialized(false)
{
}

TemporalBlending::~TemporalBlending()
{
    SafeRelease(std::move(m_inputFramebuffer));

    SafeRelease(std::move(m_performBlending));
    SafeRelease(std::move(m_descriptorTable));

    g_safeDeleter->SafeRelease(std::move(m_resultTexture));
    g_safeDeleter->SafeRelease(std::move(m_historyTexture));
}

void TemporalBlending::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    if (m_inputFramebuffer.IsValid())
    {
        DeferCreate(m_inputFramebuffer);
    }

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();

    m_onGbufferResolutionChanged = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u newSize)
        {
            Resize_Internal(newSize);
        });

    m_isInitialized = true;
}

void TemporalBlending::Resize(Vec2u newSize)
{
    PUSH_RENDER_COMMAND(RecreateTemporalBlendingFramebuffer, *this, newSize);
    HYP_SYNC_RENDER();
}

void TemporalBlending::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_extent == newSize)
    {
        return;
    }

    m_extent = newSize;

    if (!m_isInitialized)
    {
        return;
    }

    SafeRelease(std::move(m_performBlending));
    SafeRelease(std::move(m_descriptorTable));

    g_safeDeleter->SafeRelease(std::move(m_resultTexture));
    g_safeDeleter->SafeRelease(std::move(m_historyTexture));

    CreateImageOutputs();
    CreateDescriptorSets();
    CreateComputePipelines();
}

ShaderProperties TemporalBlending::GetShaderProperties() const
{
    ShaderProperties shaderProperties;

    switch (m_imageFormat)
    {
    case TF_RGBA8:
        shaderProperties.Set("OUTPUT_RGBA8");
        break;
    case TF_RGBA16F:
        shaderProperties.Set("OUTPUT_RGBA16F");
        break;
    case TF_RGBA32F:
        shaderProperties.Set("OUTPUT_RGBA32F");
        break;
    default:
        HYP_NOT_IMPLEMENTED();
    }

    static const String feedbackStrings[] = { "LOW", "MEDIUM", "HIGH" };

    shaderProperties.Set("TEMPORAL_BLEND_TECHNIQUE_" + String::ToString(uint32(m_technique)));
    shaderProperties.Set("FEEDBACK_" + feedbackStrings[MathUtil::Min(uint32(m_feedback), ArraySize(feedbackStrings) - 1)]);

    return shaderProperties;
}

void TemporalBlending::CreateImageOutputs()
{
    m_resultTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_resultTexture);

    m_resultTexture->SetPersistentRenderResourceEnabled(true);

    m_historyTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        m_imageFormat,
        Vec3u(m_extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_historyTexture);

    m_historyTexture->SetPersistentRenderResourceEnabled(true);
}

void TemporalBlending::CreateDescriptorSets()
{
    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    m_descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_resultTexture,
        &m_historyTexture
    };

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const ImageViewRef& inputImageView = m_inputFramebuffer.IsValid()
            ? m_inputFramebuffer->GetAttachment(0)->GetImageView()
            : m_inputImageView;

        Assert(inputImageView != nullptr);

        // input image
        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("InImage"), inputImageView);

        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("PrevImage"), (*textures[(frameIndex + 1) % 2])->GetRenderResource().GetImageView());

        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("VelocityImage"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());

        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("SamplerLinear"), g_renderGlobalState->placeholderData->GetSamplerLinear());

        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("SamplerNearest"), g_renderGlobalState->placeholderData->GetSamplerNearest());

        m_descriptorTable->GetDescriptorSet(NAME("TemporalBlendingDescriptorSet"), frameIndex)
            ->SetElement(NAME("OutImage"), (*textures[frameIndex % 2])->GetRenderResource().GetImageView());
    }

    DeferCreate(m_descriptorTable);
}

void TemporalBlending::CreateComputePipelines()
{
    Assert(m_descriptorTable.IsValid());

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("TemporalBlending"), GetShaderProperties());
    Assert(shader.IsValid());

    m_performBlending = g_renderBackend->MakeComputePipeline(shader, m_descriptorTable);
    DeferCreate(m_performBlending);
}

void TemporalBlending::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const ImageRef& activeImage = frame->GetFrameIndex() % 2 == 0
        ? m_resultTexture->GetRenderResource().GetImage()
        : m_historyTexture->GetRenderResource().GetImage();

    frame->renderQueue.Add<InsertBarrier>(activeImage, RS_UNORDERED_ACCESS);

    const Vec3u& extent = activeImage->GetExtent();
    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RB_OPAQUE)
                                             .GetGBufferAttachment(GTN_DEPTH)
                                             ->GetImage()
                                             ->GetExtent();

    struct
    {
        Vec2u outputDimensions;
        Vec2u depthTextureDimensions;
        uint32 blendingFrameCounter;
    } pushConstants;

    pushConstants.outputDimensions = Vec2u { extent.x, extent.y };
    pushConstants.depthTextureDimensions = Vec2u { depthTextureDimensions.x, depthTextureDimensions.y };

    pushConstants.blendingFrameCounter = m_blendingFrameCounter;

    m_performBlending->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue.Add<BindComputePipeline>(m_performBlending);

    frame->renderQueue.Add<BindDescriptorTable>(
        m_descriptorTable,
        m_performBlending,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) } } } },
        frame->GetFrameIndex());

    const uint32 viewDescriptorSetIndex = m_performBlending->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->renderQueue.Add<BindDescriptorSet>(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            m_performBlending,
            ArrayMap<Name, uint32> {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue.Add<DispatchCompute>(
        m_performBlending,
        Vec3u {
            (extent.x + 7) / 8,
            (extent.y + 7) / 8,
            1 });

    // set it to be able to be used as texture2D for next pass, or outside of this
    frame->renderQueue.Add<InsertBarrier>(activeImage, RS_SHADER_RESOURCE);

    m_blendingFrameCounter = m_technique == TemporalBlendTechnique::TECHNIQUE_4
        ? m_blendingFrameCounter + 1
        : 0;
}

void TemporalBlending::ResetProgressiveBlending()
{
    // roll over to 0 on next increment to add an extra frame
    m_blendingFrameCounter = MathUtil::MaxSafeValue(m_blendingFrameCounter);
}

} // namespace hyperion
