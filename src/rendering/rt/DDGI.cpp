/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderImage.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>
#include <Types.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/utilities/ByteUtil.hpp>

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
        HYPERION_BUBBLE_ERRORS(uniformBuffer->Create());
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
        HYPERION_BUBBLE_ERRORS(radianceBuffer->Create());
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
    m_shader.Reset();

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
    CreatePipelines();

    PUSH_RENDER_COMMAND(
        SetDDGIDescriptors,
        m_uniformBuffer,
        m_irradianceImageView,
        m_depthImageView);
}

void DDGI::CreatePipelines()
{
    m_shader = g_shaderManager->GetOrCreate(NAME("DDGI"));
    Assert(m_shader.IsValid());

    const DescriptorTableDeclaration& raytracingPipelineDescriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef raytracingPipelineDescriptorTable = g_renderBackend->MakeDescriptorTable(&raytracingPipelineDescriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        Assert(m_topLevelAccelerationStructures[frameIndex] != nullptr);

        const DescriptorSetRef& descriptorSet = raytracingPipelineDescriptorTable->GetDescriptorSet(NAME("DDGIDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("TLAS"), m_topLevelAccelerationStructures[frameIndex]);

        descriptorSet->SetElement(NAME("LightsBuffer"), g_renderGlobalState->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), m_topLevelAccelerationStructures[frameIndex]->GetMeshDescriptionsBuffer());

        descriptorSet->SetElement(NAME("DDGIUniforms"), m_uniformBuffer);
        descriptorSet->SetElement(NAME("ProbeRayData"), m_radianceBuffer);
    }

    DeferCreate(raytracingPipelineDescriptorTable);

    // Create raytracing pipeline

    m_pipeline = g_renderBackend->MakeRaytracingPipeline(
        m_shader,
        raytracingPipelineDescriptorTable);

    DeferCreate(m_pipeline);

    ShaderRef updateIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateIrradiance"));
    ShaderRef updateDepthShader = g_shaderManager->GetOrCreate(NAME("RTProbeUpdateDepth"));
    ShaderRef copyBorderTexelsIrradianceShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsIrradiance"));
    ShaderRef copyBorderTexelsDepthShader = g_shaderManager->GetOrCreate(NAME("RTCopyBorderTexelsDepth"));

    Pair<ShaderRef, ComputePipelineRef&> shaders[] = {
        { updateIrradianceShader, m_updateIrradiance },
        { updateDepthShader, m_updateDepth },
        { copyBorderTexelsIrradianceShader, m_copyBorderTexelsIrradiance },
        { copyBorderTexelsDepthShader, m_copyBorderTexelsDepth }
    };

    for (const Pair<ShaderRef, ComputePipelineRef&>& it : shaders)
    {
        Assert(it.first.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = it.first->GetCompiledShader()->GetDescriptorTableDeclaration();

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

        it.second = g_renderBackend->MakeComputePipeline(
            it.first,
            descriptorTable);

        DeferCreate(it.second);
    }
}

void DDGI::CreateUniformBuffer()
{
    m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(DDGIUniforms));

    const Vec2u gridImageDimensions = m_gridInfo.GetImageDimensions();
    const Vec3u numProbesPerDimension = m_gridInfo.NumProbesPerDimension();

    m_uniforms = DDGIUniforms {
        .aabbMax = Vec4f(m_gridInfo.aabb.max, 1.0f),
        .aabbMin = Vec4f(m_gridInfo.aabb.min, 1.0f),
        .probeBorder = {
            m_gridInfo.probeBorder.x,
            m_gridInfo.probeBorder.y,
            m_gridInfo.probeBorder.z,
            0 },
        .probeCounts = { numProbesPerDimension.x, numProbesPerDimension.y, numProbesPerDimension.z, 0 },
        .gridDimensions = { gridImageDimensions.x, gridImageDimensions.y, 0, 0 },
        .imageDimensions = { m_irradianceImage->GetExtent().x, m_irradianceImage->GetExtent().y, m_depthImage->GetExtent().x, m_depthImage->GetExtent().y },
        .params = { ByteUtil::PackFloat(m_gridInfo.probeDistance), m_gridInfo.numRaysPerProbe, PROBE_SYSTEM_FLAGS_FIRST_RUN, 0 }
    };

    PUSH_RENDER_COMMAND(
        CreateDDGIUniformBuffer,
        m_uniformBuffer,
        m_uniforms);
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    m_radianceBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, m_gridInfo.GetImageDimensions().x * m_gridInfo.GetImageDimensions().y * sizeof(ProbeRayData));

    PUSH_RENDER_COMMAND(
        CreateDDGIRadianceBuffer,
        m_radianceBuffer,
        m_gridInfo);

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

void DDGI::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags)
    {
        return;
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = m_pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("DDGIDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE)
        {
            // update acceleration structure in descriptor set
            descriptorSet->SetElement(NAME("TLAS"), m_topLevelAccelerationStructures[frameIndex]);
        }

        if (flags & RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS)
        {
            // update mesh descriptions buffer in descriptor set
            descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), m_topLevelAccelerationStructures[frameIndex]->GetMeshDescriptionsBuffer());
        }

        descriptorSet->Update();

        m_updates[frameIndex] &= ~PROBE_SYSTEM_UPDATES_TLAS;
    }
}

void DDGI::UpdateUniforms(FrameBase* frame)
{
    // FIXME: Lights are now stored per-view.
    // We don't have a View for DDGI since it is for the entire RenderWorld it is indirectly attached to.
    // We'll need to find a way to get the lights for the current view.
    // Ideas:
    // a) create a View for the DDGI and use that to get the lights. It will need to collect the lights on the Game thread so we'll need to add some kind of System to do that.
    // b) add a function to the RenderScene to get all the lights in the scene and use that to get the lights for the current view. This has a drawback that we will always have some RenderLight active when it could be inactive if it is not in any view.
    // OR: We can just use the lights in the current view and ignore the rest. This is a bit of a hack but it will work for now.
    HYP_NOT_IMPLEMENTED();

    m_uniforms.params[3] = 0;

    m_uniformBuffer->Copy(sizeof(DDGIUniforms), &m_uniforms);

    m_uniforms.params[2] &= ~PROBE_SYSTEM_FLAGS_FIRST_RUN;
}

void DDGI::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    Assert(renderSetup.IsValid());
    Assert(renderSetup.HasView());
    Assert(renderSetup.passData != nullptr);

    UpdateUniforms(frame);

    frame->renderQueue.Add<InsertBarrier>(m_radianceBuffer, RS_UNORDERED_ACCESS);

    m_randomGenerator.Next();

    struct
    {
        Matrix4 matrix;
        uint32 time;
    } pushConstants;

    pushConstants.matrix = m_randomGenerator.matrix;
    pushConstants.time = m_time++;

    m_pipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue.Add<BindRaytracingPipeline>(m_pipeline);

    frame->renderQueue.Add<BindDescriptorTable>(
        m_pipeline->GetDescriptorTable(),
        m_pipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    // bind per-view descriptor sets
    frame->renderQueue.Add<BindDescriptorSet>(
        renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
        m_pipeline);

    frame->renderQueue.Add<TraceRays>(m_pipeline, Vec3u { m_gridInfo.NumProbes(), m_gridInfo.numRaysPerProbe, 1u });

    frame->renderQueue.Add<InsertBarrier>(m_radianceBuffer, RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes

    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    frame->renderQueue.Add<InsertBarrier>(m_irradianceImage, RS_UNORDERED_ACCESS);

    frame->renderQueue.Add<InsertBarrier>(m_depthImage, RS_UNORDERED_ACCESS);

    frame->renderQueue.Add<BindComputePipeline>(m_updateIrradiance);

    frame->renderQueue.Add<BindDescriptorTable>(
        m_updateIrradiance->GetDescriptorTable(),
        m_updateIrradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frame->GetFrameIndex());

    frame->renderQueue.Add<DispatchCompute>(m_updateIrradiance, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    frame->renderQueue.Add<BindComputePipeline>(m_updateDepth);

    frame->renderQueue.Add<BindDescriptorTable>(
        m_updateDepth->GetDescriptorTable(),
        m_updateDepth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue.Add<DispatchCompute>(m_updateDepth, Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

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
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()->GetBufferIndex()) },
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
                    { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(renderSetup.world->GetBufferIndex()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*renderCamera) },
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