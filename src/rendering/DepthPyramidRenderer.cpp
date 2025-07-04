/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

struct DepthPyramidUniforms
{
    Vec2u mipDimensions;
    Vec2u prevMipDimensions;
    uint32 mipLevel;
};

DepthPyramidRenderer::DepthPyramidRenderer(GBuffer* gbuffer)
    : m_gbuffer(gbuffer),
      m_isRendered(false)
{
}

DepthPyramidRenderer::~DepthPyramidRenderer()
{
    SafeRelease(std::move(m_depthImageView));

    SafeRelease(std::move(m_depthPyramid));
    SafeRelease(std::move(m_depthPyramidView));

    SafeRelease(std::move(m_depthPyramidSampler));

    SafeRelease(std::move(m_mipImageViews));
    SafeRelease(std::move(m_mipUniformBuffers));
    SafeRelease(std::move(m_mipDescriptorTables));

    SafeRelease(std::move(m_generateDepthPyramid));
}

void DepthPyramidRenderer::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    Assert(m_gbuffer != nullptr);

    const FramebufferRef& opaqueFramebuffer = m_gbuffer->GetBucket(RB_OPAQUE).GetFramebuffer();
    Assert(opaqueFramebuffer.IsValid());

    AttachmentBase* depthAttachment = opaqueFramebuffer->GetAttachment(GTN_MAX - 1);
    Assert(depthAttachment != nullptr);

    m_depthImageView = depthAttachment->GetImageView();
    Assert(m_depthImageView.IsValid());

    auto createDepthPyramidResources = [this]()
    {
        HYP_NAMED_SCOPE("Create depth pyramid resources");
        Threads::AssertOnThread(g_renderThread);

        m_depthPyramidSampler = g_renderBackend->MakeSampler(TFM_NEAREST_MIPMAP, TFM_NEAREST, TWM_CLAMP_TO_EDGE);
        HYPERION_ASSERT_RESULT(m_depthPyramidSampler->Create());

        const ImageRef& depthImage = m_depthImageView->GetImage();
        Assert(depthImage.IsValid());

        // create depth pyramid image
        m_depthPyramid = g_renderBackend->MakeImage(TextureDesc {
            TT_TEX2D,
            TF_R32F,
            Vec3u {
                depthImage->GetExtent().x > 1 ? uint32(MathUtil::NextPowerOf2(depthImage->GetExtent().x)) : 1,
                depthImage->GetExtent().y > 1 ? uint32(MathUtil::NextPowerOf2(depthImage->GetExtent().y)) : 1,
                1 },
            TFM_NEAREST_MIPMAP,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE });

        m_depthPyramid->Create();

        m_depthPyramidView = g_renderBackend->MakeImageView(m_depthPyramid);
        m_depthPyramidView->Create();

        const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();
        const Vec3u& depthPyramidExtent = m_depthPyramid->GetExtent();

        const uint32 numMipLevels = m_depthPyramid->NumMipmaps();

        m_mipImageViews.Clear();
        m_mipImageViews.Reserve(numMipLevels);

        m_mipUniformBuffers.Clear();
        m_mipUniformBuffers.Reserve(numMipLevels);

        uint32 mipWidth = imageExtent.x,
               mipHeight = imageExtent.y;

        for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
        {
            const uint32 prevMipWidth = mipWidth,
                         prevMipHeight = mipHeight;

            mipWidth = MathUtil::Max(1u, depthPyramidExtent.x >> (mipLevel));
            mipHeight = MathUtil::Max(1u, depthPyramidExtent.y >> (mipLevel));

            DepthPyramidUniforms uniforms;
            uniforms.mipDimensions = { mipWidth, mipHeight };
            uniforms.prevMipDimensions = { prevMipWidth, prevMipHeight };
            uniforms.mipLevel = mipLevel;

            GpuBufferRef& mipUniformBuffer = m_mipUniformBuffers.PushBack(g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(DepthPyramidUniforms)));
            HYPERION_ASSERT_RESULT(mipUniformBuffer->Create());
            mipUniformBuffer->Copy(sizeof(DepthPyramidUniforms), &uniforms);

            ImageViewRef& mipImageView = m_mipImageViews.PushBack(g_renderBackend->MakeImageView(m_depthPyramid, mipLevel, 1, 0, m_depthPyramid->NumFaces()));
            HYPERION_ASSERT_RESULT(mipImageView->Create());
        }

        ShaderRef shader = g_shaderManager->GetOrCreate(NAME("GenerateDepthPyramid"), {});
        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        const DescriptorSetDeclaration* depthPyramidDescriptorSetDecl = descriptorTableDecl.FindDescriptorSetDeclaration(NAME("DepthPyramidDescriptorSet"));
        Assert(depthPyramidDescriptorSetDecl != nullptr);

        while (m_mipDescriptorTables.Size() > numMipLevels)
        {
            SafeRelease(m_mipDescriptorTables.PopFront());
        }

        while (m_mipDescriptorTables.Size() < numMipLevels)
        {
            m_mipDescriptorTables.PushFront(DescriptorTableRef {});
        }

        for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
        {
            DescriptorTableRef& descriptorTable = m_mipDescriptorTables[mipLevel];

            const auto setDescriptorSetElements = [&]()
            {
                for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
                {
                    const DescriptorSetRef& depthPyramidDescriptorSet = descriptorTable->GetDescriptorSet(NAME("DepthPyramidDescriptorSet"), frameIndex);
                    Assert(depthPyramidDescriptorSet != nullptr);

                    if (mipLevel == 0)
                    {
                        // first mip level -- input is the actual depth image
                        depthPyramidDescriptorSet->SetElement(NAME("InImage"), m_depthImageView);
                    }
                    else
                    {
                        Assert(m_mipImageViews[mipLevel - 1] != nullptr);

                        depthPyramidDescriptorSet->SetElement(NAME("InImage"), m_mipImageViews[mipLevel - 1]);
                    }

                    depthPyramidDescriptorSet->SetElement(NAME("OutImage"), m_mipImageViews[mipLevel]);
                    depthPyramidDescriptorSet->SetElement(NAME("UniformBuffer"), m_mipUniformBuffers[mipLevel]);
                    depthPyramidDescriptorSet->SetElement(NAME("DepthPyramidSampler"), m_depthPyramidSampler);
                }
            };

            if (!descriptorTable.IsValid())
            {
                descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

                setDescriptorSetElements();

                HYPERION_ASSERT_RESULT(descriptorTable->Create());
            }
            else
            {
                setDescriptorSetElements();

                for (uint32 frameIndex = 0; frameIndex < maxFramesInFlight; frameIndex++)
                {
                    descriptorTable->Update(frameIndex);
                }
            }
        }

        // use the first mip descriptor table to create the compute pipeline, since the descriptor set layout is the same for all mip levels
        m_generateDepthPyramid = g_renderBackend->MakeComputePipeline(shader, m_mipDescriptorTables.Front());
        DeferCreate(m_generateDepthPyramid);
    };

    createDepthPyramidResources();
}

Vec2u DepthPyramidRenderer::GetExtent() const
{
    if (!m_depthPyramid.IsValid())
    {
        return Vec2u::One();
    }

    const Vec3u extent = m_depthPyramid->GetExtent();

    return { extent.x, extent.y };
}

void DepthPyramidRenderer::Render(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    const SizeType numDepthPyramidMipLevels = m_mipImageViews.Size();

    const Vec3u& imageExtent = m_depthImageView->GetImage()->GetExtent();
    const Vec3u& depthPyramidExtent = m_depthPyramid->GetExtent();

    uint32 mipWidth = imageExtent.x,
           mipHeight = imageExtent.y;

    for (uint32 mipLevel = 0; mipLevel < numDepthPyramidMipLevels; mipLevel++)
    {
        // level 0 == write just-rendered depth image into mip 0

        // put the mip into writeable state
        frame->GetCommandList().Add<InsertBarrier>(
            m_depthPyramid,
            RS_UNORDERED_ACCESS,
            ImageSubResource { .baseMipLevel = mipLevel });

        const uint32 prevMipWidth = mipWidth,
                     prevMipHeight = mipHeight;

        mipWidth = MathUtil::Max(1u, depthPyramidExtent.x >> (mipLevel));
        mipHeight = MathUtil::Max(1u, depthPyramidExtent.y >> (mipLevel));

        frame->GetCommandList().Add<BindDescriptorTable>(
            m_mipDescriptorTables[mipLevel],
            m_generateDepthPyramid,
            ArrayMap<Name, ArrayMap<Name, uint32>> {},
            frameIndex);

        // set push constant data for the current mip level
        frame->GetCommandList().Add<BindComputePipeline>(m_generateDepthPyramid);

        frame->GetCommandList().Add<DispatchCompute>(
            m_generateDepthPyramid,
            Vec3u {
                (mipWidth + 31) / 32,
                (mipHeight + 31) / 32,
                1 });

        // put this mip into readable state
        frame->GetCommandList().Add<InsertBarrier>(
            m_depthPyramid,
            RS_SHADER_RESOURCE,
            ImageSubResource { .baseMipLevel = mipLevel });
    }

    frame->GetCommandList().Add<InsertBarrier>(
        m_depthPyramid,
        RS_SHADER_RESOURCE);

    m_isRendered = true;
}

} // namespace hyperion