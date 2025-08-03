/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderImage.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/ByteUtil.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion {

enum ProbeSystemUpdates : uint32
{
    PROBE_SYSTEM_UPDATES_NONE = 0x0,
    PROBE_SYSTEM_UPDATES_TLAS = 0x1
};

static constexpr TextureFormat ddgiIrradianceFormat = TF_RGBA16F;
static constexpr TextureFormat ddgiDepthFormat = TF_RG16F;

#pragma region Render commands

struct RENDER_COMMAND(SetDDGIDescriptors)
    : RenderCommand
{
    GpuBufferRef uniformBuffer;
    ImageViewRef irradianceImageView;
    ImageViewRef depthImageView;

    RENDER_COMMAND(SetDDGIDescriptors)(
        const GpuBufferRef& uniformBuffer,
        const ImageViewRef& irradianceImageView,
        const ImageViewRef& depthImageView)
        : uniformBuffer(uniformBuffer),
          irradianceImageView(irradianceImageView),
          depthImageView(depthImageView)
    {
    }

    virtual ~RENDER_COMMAND(SetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIUniforms"), uniformBuffer);

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIIrradianceTexture"), irradianceImageView);

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIDepthTexture"), depthImageView);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetDDGIDescriptors)
    : RenderCommand
{
    RENDER_COMMAND(UnsetDDGIDescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(UnsetDDGIDescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        // remove result image from global descriptor set
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIIrradianceTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());

            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("DDGIDepthTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIUniformBuffer)
    : RenderCommand
{
    GpuBufferRef uniformBuffer;
    DDGIUniforms uniforms;

    RENDER_COMMAND(CreateDDGIUniformBuffer)(const GpuBufferRef& uniformBuffer, const DDGIUniforms& uniforms)
        : uniformBuffer(uniformBuffer),
          uniforms(uniforms)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(uniformBuffer->Create());
        uniformBuffer->Copy(sizeof(DDGIUniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateDDGIRadianceBuffer)
    : RenderCommand
{
    GpuBufferRef radianceBuffer;
    DDGIInfo gridInfo;

    RENDER_COMMAND(CreateDDGIRadianceBuffer)(const GpuBufferRef& radianceBuffer, const DDGIInfo& gridInfo)
        : radianceBuffer(radianceBuffer),
          gridInfo(gridInfo)
    {
    }

    virtual ~RENDER_COMMAND(CreateDDGIRadianceBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(radianceBuffer->Create());
        radianceBuffer->Memset(radianceBuffer->Size(), 0);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

DDGI::DDGI(DDGIInfo&& gridInfo)
    : m_gridInfo(std::move(gridInfo)),
      m_updates { PROBE_SYSTEM_UPDATES_NONE, PROBE_SYSTEM_UPDATES_NONE },
      m_time(0)
{
}

DDGI::~DDGI()
{
    SafeRelease(std::move(m_uniformBuffer));
    SafeRelease(std::move(m_radianceBuffer));
    SafeRelease(std::move(m_irradianceImage));
    SafeRelease(std::move(m_irradianceImageView));
    SafeRelease(std::move(m_depthImage));
    SafeRelease(std::move(m_depthImageView));
    SafeRelease(std::move(m_pipeline));
    SafeRelease(std::move(m_updateIrradiance));
    SafeRelease(std::move(m_updateDepth));
    SafeRelease(std::move(m_copyBorderTexelsIrradiance));
    SafeRelease(std::move(m_copyBorderTexelsDepth));

    PUSH_RENDER_COMMAND(UnsetDDGIDescriptors);
}

void DDGI::Create()
{
    const Vec3u grid = m_gridInfo.NumProbesPerDimension();
    m_probes.Resize(m_gridInfo.NumProbes());

    for (uint32 x = 0; x < grid.x; x++)
    {
        for (uint32 y = 0; y < grid.y; y++)
        {
            for (uint32 z = 0; z < grid.z; z++)
            {
                const uint32 index = x * grid.x * grid.y + y * grid.z + z;

                m_probes[index] = Probe {
                    (Vec3f { float(x), float(y), float(z) } - (Vec3f(m_gridInfo.probeBorder) * 0.5f)) * m_gridInfo.probeDistance
                };
            }
        }
    }

    CreateStorageBuffers();
    CreateUniformBuffer();

    PUSH_RENDER_COMMAND(
        SetDDGIDescriptors,
        m_uniformBuffer,
        m_irradianceImageView,
        m_depthImageView);
}

void DDGI::CreateUniformBuffer()
{
    m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms));

    const Vec2u gridImageDimensions = m_gridInfo.GetImageDimensions();
    const Vec3u numProbesPerDimension = m_gridInfo.NumProbesPerDimension();

    m_uniforms = DDGIUniforms {
        .aabbMax = Vec4f(m_gridInfo.aabb.max, 1.0f),
        .aabbMin = Vec4f(m_gridInfo.aabb.min, 1.0f),
        .probeBorder = Vec4u(m_gridInfo.probeBorder, 0),
        .probeCounts = { numProbesPerDimension.x, numProbesPerDimension.y, numProbesPerDimension.z, 0 },
        .gridDimensions = { gridImageDimensions.x, gridImageDimensions.y, 0, 0 },
        .imageDimensions = { m_irradianceImage->GetExtent().x, m_irradianceImage->GetExtent().y, m_depthImage->GetExtent().x, m_depthImage->GetExtent().y },
        .probeDistance = m_gridInfo.probeDistance,
        .numRaysPerProbe = m_gridInfo.numRaysPerProbe,
        .numBoundLights = 0,
        .flags = PROBE_SYSTEM_FLAGS_FIRST_RUN
    };

    PUSH_RENDER_COMMAND(CreateDDGIUniformBuffer, m_uniformBuffer, m_uniforms);
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    m_radianceBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, m_gridInfo.GetImageDimensions().x * m_gridInfo.GetImageDimensions().y * sizeof(ProbeRayData));

    PUSH_RENDER_COMMAND(CreateDDGIRadianceBuffer, m_radianceBuffer, m_gridInfo);

    { // irradiance image
        const Vec3u extent {
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_irradianceImage = g_renderBackend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgiIrradianceFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_irradianceImage);
    }

    { // irradiance image view
        m_irradianceImageView = g_renderBackend->MakeImageView(m_irradianceImage);

        DeferCreate(m_irradianceImageView);
    }

    { // depth image
        const Vec3u extent {
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_depthImage = g_renderBackend->MakeImage(TextureDesc {
            TT_TEX2D,
            ddgiDepthFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        DeferCreate(m_depthImage);
    }

    { // depth image view
        m_depthImageView = g_renderBackend->MakeImageView(m_depthImage);

        DeferCreate(m_depthImageView);
    }
}

void DDGI::UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    DeferredPassData* pd = static_cast<DeferredPassData*>(renderSetup.passData);
    Assert(pd != nullptr);

    const auto setDescriptorElements = [this, pd](DescriptorSetBase* descriptorSet, const TLASRef& tlas)
        {
            Assert(tlas != nullptr);

            descriptorSet->SetElement(NAME("TLAS"), tlas);
            descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), tlas->GetMeshDescriptionsBuffer());
            descriptorSet->SetElement(NAME("DDGIUniforms"), m_uniformBuffer);
            descriptorSet->SetElement(NAME("ProbeRayData"), m_radianceBuffer);
        };

    if (m_pipeline != nullptr)
    {
        DescriptorSetBase* descriptorSet = m_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DDGIDescriptorSet"), frame->GetFrameIndex());
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->topLevelAccelerationStructures[frame->GetFrameIndex()]);

        descriptorSet->UpdateDirtyState();
        descriptorSet->Update(true); //! temp

        return;
    }

    // Create raytracing pipeline
    ShaderRef raytracingShader = g_shaderManager->GetOrCreate(NAME("DDGI"));
    Assert(raytracingShader != nullptr);

    const DescriptorTableDeclaration& descriptorTableDecl = raytracingShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        DescriptorSetBase* descriptorSet = descriptorTable->GetDescriptorSet(NAME("DDGIDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        setDescriptorElements(descriptorSet, pd->topLevelAccelerationStructures[frameIndex]);
    }

    HYP_GFX_ASSERT(descriptorTable->Create());

    m_pipeline = g_renderBackend->MakeRaytracingPipeline(raytracingShader, descriptorTable);
    HYP_GFX_ASSERT(m_pipeline->Create());
    
    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        descriptorTable->Update(frameIndex, /* force */ true);
    }

    // Create compute pipelines
    ShaderRef updateIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateIrradiance"));
    ShaderRef updateDepthShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateDepth"));
    ShaderRef copyBorderTexelsIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsIrradiance"));
    ShaderRef copyBorderTexelsDepthShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsDepth"));

    Pair<ShaderRef, ComputePipelineRef&> computePipelines[] = {
        { updateIrradianceShader, m_updateIrradiance },
        { updateDepthShader, m_updateDepth },
        { copyBorderTexelsIrradianceShader, m_copyBorderTexelsIrradiance },
        { copyBorderTexelsDepthShader, m_copyBorderTexelsDepth }
    };

    for (auto& [shader, computePipeline] : computePipelines)
    {
        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("DDGIDescriptorSet"), frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement(NAME("DDGIUniforms"), m_uniformBuffer);
            descriptorSet->SetElement(NAME("ProbeRayData"), m_radianceBuffer);

            descriptorSet->SetElement(NAME("OutputIrradianceImage"), m_irradianceImageView);
            descriptorSet->SetElement(NAME("OutputDepthImage"), m_depthImageView);
        }

        DeferCreate(descriptorTable);

        computePipeline = g_renderBackend->MakeComputePipeline(shader, descriptorTable);
        DeferCreate(computePipeline);
    }
}

void DDGI::UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup)
{
    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    DDGIUniforms& uniforms = m_uniforms;

    uint32 numBoundLights = 0;

    const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (numBoundLights >= maxBoundLights)
        {
            break;
        }

        uniforms.lightIndices[numBoundLights++] = RenderApi_RetrieveResourceBinding(light);
    }

    uniforms.numBoundLights = numBoundLights;

    m_uniformBuffer->Copy(sizeof(DDGIUniforms), &m_uniforms);

    uniforms.flags &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(renderSetup.IsValid());
    Assert(renderSetup.HasView());
    Assert(renderSetup.passData != nullptr);

    UpdatePipelineState(frame, renderSetup);
    UpdateUniforms(frame, renderSetup);

    m_randomGenerator.Next();

    struct
    {
        Matrix4 matrix;
        uint32 time;
    } pushConstants;

    pushConstants.matrix = m_randomGenerator.matrix;
    pushConstants.time = m_time++;

    m_pipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    frame->renderQueue << BindRaytracingPipeline(m_pipeline);

    frame->renderQueue << BindDescriptorTable(
        m_pipeline->GetDescriptorTable(),
        m_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << TraceRays(m_pipeline, Vec3u { m_gridInfo.NumProbes(), m_gridInfo.numRaysPerProbe, 1u });

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes

    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(m_updateIrradiance);

    frame->renderQueue << BindDescriptorTable(
        m_updateIrradiance->GetDescriptorTable(),
        m_updateIrradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(m_updateIrradiance, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    frame->renderQueue << BindComputePipeline(m_updateDepth);

    frame->renderQueue << BindDescriptorTable(
        m_updateDepth->GetDescriptorTable(),
        m_updateDepth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(m_updateDepth, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    m_irradianceImage->InsertBarrier(
        frame->renderQueue,
        RS_UNORDERED_ACCESS
    );

    m_depthImage->InsertBarrier(
        frame->renderQueue,
        RS_UNORDERED_ACCESS
    );

    // now copy border texels
    m_copyBorderTexelsIrradiance->Bind(frame->renderQueue);

    m_copyBorderTexelsIrradiance->GetDescriptorTable()->Bind(
        frame,
        m_copyBorderTexelsIrradiance,
        {
            {
                NAME("Global"),
                {
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(envProbe.Get(), 0) }
                }
            }
        }
    );

    m_copyBorderTexelsIrradiance->Dispatch(
        frame->renderQueue,
        Vec3u {
            (probeCounts.x * probeCounts.y * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.x)) + 7 / 8,
            (probeCounts.z * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.z)) + 7 / 8,
            1u
        }
    );
    
    m_copyBorderTexelsDepth->Bind(frame->renderQueue);

    m_copyBorderTexelsDepth->GetDescriptorTable()->Bind(
        frame,
        m_copyBorderTexelsDepth,
        {
            {
                NAME("Global"),
                {
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*renderSetup.camera) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(envProbe.Get(), 0) }
                }
            }
        }
    );
    
    m_copyBorderTexelsDepth->Dispatch(
        frame->renderQueue,
        Vec3u {
            (probeCounts.x * probeCounts.y * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.x)) + 15 / 16,
            (probeCounts.z * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.z)) + 15 / 16,
            1u
        }
    );

    m_irradianceImage->InsertBarrier(
        frame->renderQueue,
        RS_SHADER_RESOURCE
    );

    m_depthImage->InsertBarrier(
        frame->renderQueue,
        RS_SHADER_RESOURCE
    );
#endif
}

HYP_DESCRIPTOR_CBUFF(Global, DDGIUniforms, 1, sizeof(DDGIUniforms), false);
HYP_DESCRIPTOR_SRV(Global, DDGIIrradianceTexture, 1);
HYP_DESCRIPTOR_SRV(Global, DDGIDepthTexture, 1);

} // namespace hyperion
