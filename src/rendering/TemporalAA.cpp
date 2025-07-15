/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/TemporalAA.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderFramebuffer.hpp>

#include <scene/Texture.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

TemporalAA::TemporalAA(const ImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer)
    : m_inputImageView(inputImageView),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_isInitialized(false)
{
}

TemporalAA::~TemporalAA()
{
    SafeRelease(std::move(m_inputImageView));
    SafeRelease(std::move(m_computeTaa));
}

void TemporalAA::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    CreateImages();
    CreateComputePipelines();

    m_onGbufferResolutionChanged = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u newSize)
        {
            SafeRelease(std::move(m_computeTaa));

            CreateComputePipelines();
        });

    m_isInitialized = true;
}

void TemporalAA::CreateImages()
{
    m_resultTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_resultTexture);

    m_resultTexture->SetPersistentRenderResourceEnabled(true);

    m_historyTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    InitObject(m_historyTexture);

    m_historyTexture->SetPersistentRenderResourceEnabled(true);
}

void TemporalAA::CreateComputePipelines()
{
    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("TemporalAA"));
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    const FixedArray<Handle<Texture>*, 2> textures = {
        &m_resultTexture,
        &m_historyTexture
    };

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        // create descriptor sets for depth pyramid generation.
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("TemporalAADescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InColorTexture"), m_inputImageView);
        descriptorSet->SetElement(NAME("InPrevColorTexture"), (*textures[(frameIndex + 1) % 2])->GetRenderResource().GetImageView());

        descriptorSet->SetElement(NAME("InVelocityTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());

        descriptorSet->SetElement(NAME("InDepthTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());

        descriptorSet->SetElement(NAME("SamplerLinear"), g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement(NAME("SamplerNearest"), g_renderGlobalState->placeholderData->GetSamplerNearest());

        descriptorSet->SetElement(NAME("OutColorImage"), (*textures[frameIndex % 2])->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptorTable);

    m_computeTaa = g_renderBackend->MakeComputePipeline(
        shader,
        descriptorTable);

    DeferCreate(m_computeTaa);
}

void TemporalAA::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Temporal AA");

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    const ImageRef& activeImage = frame->GetFrameIndex() % 2 == 0
        ? m_resultTexture->GetRenderResource().GetImage()
        : m_historyTexture->GetRenderResource().GetImage();

    frame->renderQueue.Add<InsertBarrier>(activeImage, RS_UNORDERED_ACCESS);

    struct
    {
        Vec2u dimensions;
        Vec2u depthTextureDimensions;
        Vec2f cameraNearFar;
    } pushConstants;

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RB_OPAQUE)
                                             .GetGBufferAttachment(GTN_DEPTH)
                                             ->GetImage()
                                             ->GetExtent();

    pushConstants.dimensions = m_extent;
    pushConstants.depthTextureDimensions = Vec2u { depthTextureDimensions.x, depthTextureDimensions.y };
    pushConstants.cameraNearFar = Vec2f { renderSetup.view->GetCamera()->GetBufferData().cameraNear, renderSetup.view->GetCamera()->GetBufferData().cameraFar };

    m_computeTaa->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue.Add<BindComputePipeline>(m_computeTaa);

    frame->renderQueue.Add<BindDescriptorTable>(
        m_computeTaa->GetDescriptorTable(),
        m_computeTaa,
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frameIndex);

    frame->renderQueue.Add<DispatchCompute>(m_computeTaa, Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->renderQueue.Add<InsertBarrier>(activeImage, RS_SHADER_RESOURCE);
}

} // namespace hyperion