/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/rt/DDGI.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Texture.hpp>

#include <scene/View.hpp>
#include <scene/Light.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

enum RTRadianceUpdates : uint32
{
    RT_RADIANCE_UPDATES_NONE = 0x0,
    RT_RADIANCE_UPDATES_TLAS = 0x1,
    RT_RADIANCE_UPDATES_SHADOW_MAP = 0x2
};

#pragma region Render commands

struct RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)
    : RenderCommand
{
    ImageViewRef imageView;

    RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)(
        const ImageViewRef& imageView)
        : imageView(imageView)
    {
    }

    virtual ~RENDER_COMMAND(SetRTRadianceImageInGlobalDescriptorSet)() override
    {
        SafeRelease(std::move(imageView));
    }

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)
                ->SetElement(NAME("RTRadianceResultTexture"), imageView);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)
    : RenderCommand
{
    virtual ~RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        // remove result image from global descriptor set
        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("RTRadianceResultTexture"), g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion Render commands

RaytracingReflections::RaytracingReflections(RaytracingReflectionsConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_updates { RT_RADIANCE_UPDATES_NONE, RT_RADIANCE_UPDATES_NONE }
{
}

RaytracingReflections::~RaytracingReflections()
{
    m_shader.Reset();

    SafeRelease(std::move(m_raytracingPipeline));

    SafeRelease(std::move(m_uniformBuffers));

    // remove result image from global descriptor set
    g_safeDeleter->SafeRelease(std::move(m_texture));

    PUSH_RENDER_COMMAND(UnsetRTRadianceImageInGlobalDescriptorSet);
}

void RaytracingReflections::Create()
{
    CreateImages();
    CreateUniformBuffer();
    CreateTemporalBlending();
    CreateRaytracingPipeline();
}

void RaytracingReflections::UpdateUniforms(FrameBase* frame, const RenderSetup& renderSetup)
{
    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RTRadianceUniforms uniforms {};

    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    uniforms.minRoughness = 0.4f;
    uniforms.outputImageResolution = Vec2i(m_config.extent);

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

    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
}

void RaytracingReflections::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    UpdateUniforms(frame, renderSetup);

    const uint32 viewDescriptorSetIndex = m_raytracingPipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));
    AssertDebug(viewDescriptorSetIndex != ~0u);

    frame->renderQueue << BindRaytracingPipeline(m_raytracingPipeline);

    frame->renderQueue << BindDescriptorTable(
        m_raytracingPipeline->GetDescriptorTable(),
        m_raytracingPipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(renderSetup.envGrid, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << BindDescriptorSet(
        renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
        m_raytracingPipeline,
        ArrayMap<Name, uint32> {},
        viewDescriptorSetIndex);

    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_UNORDERED_ACCESS);

    const Vec3u imageExtent = m_texture->GetGpuImage()->GetExtent();

    const SizeType numPixels = imageExtent.Volume();
    // const SizeType halfNumPixels = numPixels / 2;

    frame->renderQueue << TraceRays(m_raytracingPipeline, Vec3u { uint32(numPixels), 1, 1 });
    frame->renderQueue << InsertBarrier(m_texture->GetGpuImage(), RS_SHADER_RESOURCE);

    // Reset progressive blending if the camera view matrix has changed (for path tracing)
    if (IsPathTracer())
    {
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(RenderApi_GetRenderProxy(renderSetup.view->GetCamera()));
        Assert(cameraProxy != nullptr);

        if (cameraProxy->bufferData.view != m_previousViewMatrix)
        {
            m_temporalBlending->ResetProgressiveBlending();

            m_previousViewMatrix = cameraProxy->bufferData.view;
        }
    }

    m_temporalBlending->Render(frame, renderSetup);
}

void RaytracingReflections::CreateImages()
{
    m_texture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u { m_config.extent, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_SAMPLED | IU_STORAGE });

    InitObject(m_texture);
}

void RaytracingReflections::CreateUniformBuffer()
{
    RTRadianceUniforms uniforms;
    Memory::MemSet(&uniforms, 0, sizeof(uniforms));

    m_uniformBuffers = {
        g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms)),
        g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(RTRadianceUniforms))
    };

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex]->SetDebugName(NAME_FMT("RaytracingReflectionsUniformBuffer_{}", frameIndex));

        HYP_GFX_ASSERT(m_uniformBuffers[frameIndex]->Create());
        m_uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);
    }
}

void RaytracingReflections::ApplyTLASUpdates(RTUpdateStateFlags flags)
{
    if (!flags)
    {
        return;
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = m_raytracingPipeline->GetDescriptorTable()
                                                    ->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frameIndex);

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

        m_updates[frameIndex] |= RT_RADIANCE_UPDATES_TLAS;
    }
}

void RaytracingReflections::CreateRaytracingPipeline()
{
    if (IsPathTracer())
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("PathTracer"));
    }
    else
    {
        m_shader = g_shaderManager->GetOrCreate(NAME("RTRadiance"));
    }

    Assert(m_shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        Assert(m_topLevelAccelerationStructures[frameIndex] != nullptr);

        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("RTRadianceDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("TLAS"), m_topLevelAccelerationStructures[frameIndex]);
        descriptorSet->SetElement(NAME("MeshDescriptionsBuffer"), m_topLevelAccelerationStructures[frameIndex]->GetMeshDescriptionsBuffer());

        descriptorSet->SetElement(NAME("OutputImage"), g_renderBackend->GetTextureImageView(m_texture));

        descriptorSet->SetElement(NAME("LightsBuffer"), g_renderGlobalState->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("MaterialsBuffer"), g_renderGlobalState->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("RTRadianceUniforms"), m_uniformBuffers[frameIndex]);
    }

    DeferCreate(descriptorTable);

    m_raytracingPipeline = g_renderBackend->MakeRaytracingPipeline(
        m_shader,
        descriptorTable);

    DeferCreate(m_raytracingPipeline);

    PUSH_RENDER_COMMAND(
        SetRTRadianceImageInGlobalDescriptorSet,
        g_renderBackend->GetTextureImageView(m_temporalBlending->GetResultTexture()));
}

void RaytracingReflections::CreateTemporalBlending()
{
    m_temporalBlending = MakeUnique<TemporalBlending>(
        m_config.extent,
        TF_RGBA16F,
        IsPathTracer()
            ? TemporalBlendTechnique::TECHNIQUE_4 // progressive blending
            : TemporalBlendTechnique::TECHNIQUE_1,
        IsPathTracer()
            ? TemporalBlendFeedback::HIGH
            : TemporalBlendFeedback::HIGH,
        g_renderBackend->GetTextureImageView(m_texture),
        m_gbuffer);

    m_temporalBlending->Create();
}

} // namespace hyperion