/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderImageView.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/AsyncCompute.hpp>

#include <rendering/Texture.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

static constexpr Vec2u shNumSamples = { 16, 16 };
static constexpr Vec2u shNumTiles = { 16, 16 };
static constexpr uint32 shNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(shNumSamples.Max()) + 1));
static constexpr bool shParallelReduce = false;

static FixedArray<Matrix4, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Matrix4, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Matrix4::LookAt(
            origin,
            origin + Texture::cubemapDirections[i].first,
            Texture::cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region RenderEnvProbe

RenderEnvProbe::RenderEnvProbe(EnvProbe* envProbe)
    : m_envProbe(envProbe),
      m_bufferData {},
      m_textureSlot(~0u)
{
    if (!m_envProbe->IsControlledByEnvGrid())
    {
        CreateShader();
    }
}

RenderEnvProbe::~RenderEnvProbe()
{
    m_renderView.Reset();
    m_shadowMap.Reset();

    SafeRelease(std::move(m_shader));
}

void RenderEnvProbe::SetPositionInGrid(const Vec4i& positionInGrid)
{
    HYP_SCOPE;

    Execute([this, positionInGrid]()
        {
            m_positionInGrid = positionInGrid;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetTextureSlot(uint32 textureSlot)
{
    HYP_SCOPE;

    Execute([this, textureSlot]()
        {
            HYP_LOG(Rendering, Debug, "Setting texture slot for EnvProbe {} (type: {}) to {}", m_envProbe->Id(), m_envProbe->GetEnvProbeType(), textureSlot);

            if (m_textureSlot == textureSlot)
            {
                return;
            }

            m_textureSlot = textureSlot;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetBufferData(const EnvProbeShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            // TEMP hack: save previous textureIndex and positionInGrid
            const Vec4i positionInGrid = m_bufferData.positionInGrid;

            m_bufferData = bufferData;

            // restore previous textureIndex and positionInGrid
            m_bufferData.textureIndex = m_textureSlot;
            m_bufferData.positionInGrid = positionInGrid;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::SetViewResourceHandle(TResourceHandle<RenderView>&& renderView)
{
    HYP_SCOPE;

    Execute([this, renderView = std::move(renderView)]()
        {
            if (m_renderView == renderView)
            {
                return;
            }

            m_renderView = std::move(renderView);
        });
}

void RenderEnvProbe::SetShadowMap(TResourceHandle<RenderShadowMap>&& shadowMap)
{
    HYP_SCOPE;

    Execute([this, shadowMap = std::move(shadowMap)]()
        {
            if (m_shadowMap == shadowMap)
            {
                return;
            }

            m_shadowMap = std::move(shadowMap);
        });
}

void RenderEnvProbe::SetSphericalHarmonics(const EnvProbeSphericalHarmonics& sphericalHarmonics)
{
    HYP_SCOPE;

    Execute([this, sphericalHarmonics]()
        {
            m_sphericalHarmonics = sphericalHarmonics;

            SetNeedsUpdate();
        });
}

void RenderEnvProbe::CreateShader()
{
    if (m_envProbe->IsControlledByEnvGrid())
    {
        return;
    }

    if (m_envProbe->IsReflectionProbe())
    {
        m_shader = g_shaderManager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(staticMeshVertexAttributes, { "ENV_PROBE", "WRITE_NORMALS", "WRITE_MOMENTS" }));
    }
    else if (m_envProbe->IsSkyProbe())
    {
        m_shader = g_shaderManager->GetOrCreate(
            NAME("RenderSky"),
            ShaderProperties(staticMeshVertexAttributes));
    }
    else if (m_envProbe->IsShadowProbe())
    {
        m_shader = g_shaderManager->GetOrCreate(
            NAME("RenderToCubemap"),
            ShaderProperties(staticMeshVertexAttributes, { "MODE_SHADOWS" }));
    }
    else
    {
        HYP_UNREACHABLE();
    }

    Assert(m_shader.IsValid());
}

void RenderEnvProbe::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderEnvProbe::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderEnvProbe::Update_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

GpuBufferHolderBase* RenderEnvProbe::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES];
}

void RenderEnvProbe::UpdateBufferData()
{
    HYP_SCOPE;

    /*const BoundingBox aabb = BoundingBox(m_bufferData.aabbMin.GetXYZ(), m_bufferData.aabbMax.GetXYZ());
    const Vec3f worldPosition = m_bufferData.worldPosition.GetXYZ();

    const FixedArray<Matrix4, 6> viewMatrices = CreateCubemapMatrices(aabb, worldPosition);

    EnvProbeShaderData* bufferData = static_cast<EnvProbeShaderData*>(m_bufferAddress);

    Memory::MemCpy(bufferData, &m_bufferData, sizeof(EnvProbeShaderData));
    Memory::MemCpy(bufferData->faceViewMatrices, viewMatrices.Data(), sizeof(EnvProbeShaderData::faceViewMatrices));
    Memory::MemCpy(bufferData->sh.values, m_sphericalHarmonics.values, sizeof(EnvProbeSphericalHarmonics::values));

    if (m_envProbe->IsShadowProbe())
    {
        Assert(m_shadowMap);

        bufferData->textureIndex = m_shadowMap->GetAtlasElement().pointLightIndex;
    }
    else
    {
        bufferData->textureIndex = m_textureSlot;
    }

    bufferData->positionInGrid = m_positionInGrid;

    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);*/
}

/// TEMPORARY: will be replaced by EnvProbeRenderer classes.
void RenderEnvProbe::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
#if 0
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    if (m_envProbe->IsControlledByEnvGrid())
    {
        HYP_LOG(EnvProbe, Warning, "EnvProbe {} is controlled by an EnvGrid, but Render() is being called!", m_envProbe->Id());

        return;
    }

    AssertDebug(m_bufferIndex != ~0u);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(m_renderView->GetView());

    if (!m_envProbe->NeedsRender())
    {
        return;
    }

    HYP_LOG(EnvProbe, Debug, "Rendering EnvProbe {} (type: {})",
        m_envProbe->Id(), m_envProbe->GetEnvProbeType());

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderSetup newRenderSetup = renderSetup;
    newRenderSetup.view = m_renderView.Get();

    {
        newRenderSetup.envProbe = m_envProbe;

        renderCollector.ExecuteDrawCalls(frame, newRenderSetup, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

        newRenderSetup.envProbe = nullptr;
    }

    const ViewOutputTarget& outputTarget = m_envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const ImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();

    if (m_envProbe->IsSkyProbe() || m_envProbe->IsReflectionProbe())
    {
        // HYP_LOG(EnvProbe, Debug, "Render sky/reflection probe {} (type: {})",
        //     m_envProbe->Id(), m_envProbe->GetEnvProbeType());

        return; // now handled by ReflectionProbeRenderer
    }
    else if (m_envProbe->IsShadowProbe())
    {
        Assert(m_shadowMap);
        Assert(m_shadowMap->GetAtlasElement().pointLightIndex != ~0u);

        HYP_LOG(EnvProbe, Debug, "Render shadow probe {} (pointlight index: {})",
            m_envProbe->Id(), m_shadowMap->GetAtlasElement().pointLightIndex);

        const ImageViewRef& shadowMapImageView = m_shadowMap->GetImageView();
        Assert(shadowMapImageView.IsValid());

        const ImageRef& shadowMapImage = shadowMapImageView->GetImage();
        Assert(shadowMapImage.IsValid());

        const ShadowMapAtlasElement& atlasElement = m_shadowMap->GetAtlasElement();

        // Copy combined shadow map to the final shadow map
        frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(
            shadowMapImage,
            RS_COPY_DST,
            ImageSubResource { .baseArrayLayer = atlasElement.pointLightIndex * 6, .numLayers = 6 });

        // copy the image
        for (uint32 i = 0; i < 6; i++)
        {
            frame->renderQueue << Blit(
                framebufferImage,
                shadowMapImage,
                Rect<uint32> { 0, 0, framebufferImage->GetExtent().x, framebufferImage->GetExtent().y },
                Rect<uint32> {
                    atlasElement.offsetCoords.x,
                    atlasElement.offsetCoords.y,
                    atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                    atlasElement.offsetCoords.y + atlasElement.dimensions.y },
                0,                                      /* srcMip */
                0,                                      /* dstMip */
                i,                                      /* srcFace */
                atlasElement.pointLightIndex * 6 + i /* dstFace */
            );
        }

        // put the images back into a state for reading
        frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(
            shadowMapImage,
            RS_SHADER_RESOURCE,
            ImageSubResource { .baseArrayLayer = atlasElement.pointLightIndex * 6, .numLayers = 6 });
    }

    // Temp; refactor
    m_envProbe->SetNeedsRender(false);
#endif
}

#pragma endregion RenderEnvProbe

#pragma region EnvProbeRenderer

EnvProbeRenderer::EnvProbeRenderer()
{
}

EnvProbeRenderer::~EnvProbeRenderer()
{
}

void EnvProbeRenderer::Initialize()
{
}

void EnvProbeRenderer::Shutdown()
{
}

void EnvProbeRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.envProbe != nullptr);

    EnvProbe* envProbe = renderSetup.envProbe;
    AssertDebug(envProbe != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = envProbe->GetRenderResource().GetViewRenderResourceHandle().Get();
    rs.passData = FetchViewPassData(rs.view->GetView());

    RenderProbe(frame, rs, envProbe);
}

PassData* EnvProbeRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    PassData* pd = new PassData;
    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetRenderResource().GetViewport();

    return pd;
}

#pragma endregion EnvProbeRenderer

#pragma region ReflectionProbeRenderer

ReflectionProbeRenderer::ReflectionProbeRenderer()
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Initialize()
{
    HYP_SCOPE;

    EnvProbeRenderer::Initialize();

    CreateShader();
}

void ReflectionProbeRenderer::Shutdown()
{
    HYP_SCOPE;

    EnvProbeRenderer::Shutdown();

    SafeRelease(std::move(m_shader));
}

void ReflectionProbeRenderer::CreateShader()
{
    HYP_SCOPE;

    AssertDebug(!m_shader.IsValid());

    m_shader = g_shaderManager->GetOrCreate(
        NAME("RenderToCubemap"),
        ShaderProperties(staticMeshVertexAttributes, { "WRITE_NORMALS", "WRITE_MOMENTS" }));

    Assert(m_shader.IsValid());
}

void ReflectionProbeRenderer::RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    View* view = renderSetup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = RenderApi_GetRenderCollector(view);

#ifdef HYP_DEBUG_MODE
    if (envProbe->IsA<SkyProbe>())
    {
        if (!renderSetup.light)
        {
            HYP_LOG(Rendering, Warning, "No directional light bound while rendering SkyProbe {} in view {}", envProbe->Id(), view->Id());
        }
        else if (renderSetup.light->GetLightType() != LT_DIRECTIONAL)
        {
            HYP_LOG(Rendering, Warning, "Light bound to SkyProbe pass is not a directional light: {} in view {}", renderSetup.light->Id(), view->Id());
        }
    }
#endif

    renderCollector.ExecuteDrawCalls(frame, renderSetup, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const ImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();

    if (envProbe->ShouldComputePrefilteredEnvMap())
    {
        ComputePrefilteredEnvMap(frame, renderSetup, envProbe);
    }

    if (envProbe->ShouldComputeSphericalHarmonics())
    {
        ComputeSH(frame, renderSetup, envProbe);
    }

    if (SkyProbe* skyProbe = ObjCast<SkyProbe>(envProbe))
    {
        Assert(skyProbe->GetSkyboxCubemap().IsValid());

        const ImageRef& dstImage = skyProbe->GetSkyboxCubemap()->GetRenderResource().GetImage();
        Assert(dstImage.IsValid());
        Assert(dstImage->IsCreated());

        frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);

        frame->renderQueue << Blit(framebufferImage, dstImage);

        if (dstImage->HasMipmaps())
        {
            frame->renderQueue << GenerateMipmaps(dstImage);
        }

        frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }
}

void ReflectionProbeRenderer::ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    View* view = renderSetup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    struct ConvolveProbeUniforms
    {
        Vec2u outImageDimensions;
        Vec4f worldPosition;
        uint32 numBoundLights;
        alignas(16) uint32 lightIndices[16];
    };

    ShaderProperties shaderProperties;

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set("LIGHTING");
    }

    ShaderRef convolveProbeShader = g_shaderManager->GetOrCreate(NAME("ConvolveProbe"), shaderProperties);

    if (!convolveProbeShader)
    {
        HYP_FAIL("Failed to create ConvolveProbe shader");
    }

    const Handle<Texture>& prefilteredEnvMap = envProbe->GetPrefilteredEnvMap();
    Assert(prefilteredEnvMap.IsValid());

    ConvolveProbeUniforms uniforms;
    uniforms.outImageDimensions = prefilteredEnvMap->GetExtent().GetXY();
    uniforms.worldPosition = envProbe->GetRenderResource().GetBufferData().worldPosition;

    const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);
    uint32 numBoundLights = 0;

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

    GpuBufferRef uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
    HYPERION_ASSERT_RESULT(uniformBuffer->Create());
    uniformBuffer->Copy(sizeof(uniforms), &uniforms);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);

    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* momentsAttachment = framebuffer->GetAttachment(2);

    Assert(colorAttachment != nullptr);
    Assert(normalsAttachment != nullptr);
    Assert(momentsAttachment != nullptr);

    const DescriptorTableDeclaration& descriptorTableDecl = convolveProbeShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
    descriptorTable->SetDebugName(NAME_FMT("ConvolveProbeDescriptorTable_{}", envProbe->Id().Value()));

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("ConvolveProbeDescriptorSet"), frameIndex);
        Assert(descriptorSet.IsValid());

        descriptorSet->SetElement(NAME("UniformBuffer"), uniformBuffer);
        descriptorSet->SetElement(NAME("ColorTexture"), colorAttachment->GetImageView());
        descriptorSet->SetElement(NAME("NormalsTexture"), normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement(NAME("MomentsTexture"), momentsAttachment ? momentsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement(NAME("SamplerLinear"), g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement(NAME("SamplerNearest"), g_renderGlobalState->placeholderData->GetSamplerNearest());
        descriptorSet->SetElement(NAME("OutImage"), prefilteredEnvMap->GetRenderResource().GetImageView());
    }

    HYPERION_ASSERT_RESULT(descriptorTable->Create());

    ComputePipelineRef convolveProbeComputePipeline = g_renderBackend->MakeComputePipeline(convolveProbeShader, descriptorTable);
    HYPERION_ASSERT_RESULT(convolveProbeComputePipeline->Create());

    frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetRenderResource().GetImage(), RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(convolveProbeComputePipeline);

    frame->renderQueue << BindDescriptorTable(
        descriptorTable,
        convolveProbeComputePipeline,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"), { { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(
        convolveProbeComputePipeline,
        Vec3u { (prefilteredEnvMap->GetExtent().x + 7) / 8, (prefilteredEnvMap->GetExtent().y + 7) / 8, 1 });

    if (prefilteredEnvMap->GetTextureDesc().HasMipmaps())
    {
        frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetRenderResource().GetImage(), RS_COPY_DST);
        frame->renderQueue << GenerateMipmaps(prefilteredEnvMap->GetRenderResource().GetImage());
    }

    frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);

    // for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    // {
    //     g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(NAME("Global"), frameIndex)->SetElement(NAME("EnvProbeTextures"), m_textureSlot, prefilteredEnvMap->GetRenderResource().GetImageView());
    //     HYP_LOG(EnvProbe, Debug, "Set EnvProbe texture slot {} for envprobe {} in global descriptor table",
    //         m_envProbe->GetTextureSlot(), m_envProbe->Id());
    // }

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind([delegateHandle, uniformBuffer = std::move(uniformBuffer), convolveProbeComputePipeline = std::move(convolveProbeComputePipeline), descriptorTable = std::move(descriptorTable)](...)
        {
            HYPERION_ASSERT_RESULT(uniformBuffer->Destroy());
            HYPERION_ASSERT_RESULT(convolveProbeComputePipeline->Destroy());
            HYPERION_ASSERT_RESULT(descriptorTable->Destroy());

            delete delegateHandle;
        });
}

void ReflectionProbeRenderer::ComputeSH(FrameBase* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    View* view = renderSetup.view->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const ViewOutputTarget& outputTarget = envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr);

    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    Array<GpuBufferRef> shTilesBuffers;
    shTilesBuffers.Resize(shNumLevels);

    Array<DescriptorTableRef> shTilesDescriptorTables;
    shTilesDescriptorTables.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        const SizeType size = sizeof(SHTile) * (shNumTiles.x >> i) * (shNumTiles.y >> i);

        shTilesBuffers[i] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, size);
        HYPERION_ASSERT_RESULT(shTilesBuffers[i]->Create());
    }

    ShaderProperties shaderProperties;

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Set("LIGHTING");
    }

    HashMap<Name, Pair<ShaderRef, ComputePipelineRef>> pipelines = {
        { NAME("Clear"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { "MODE_CLEAR" } })), ComputePipelineRef() } },
        { NAME("BuildCoeffs"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { "MODE_BUILD_COEFFICIENTS" } })), ComputePipelineRef() } },
        { NAME("Reduce"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { "MODE_REDUCE" } })), ComputePipelineRef() } },
        { NAME("Finalize"), { g_shaderManager->GetOrCreate(NAME("ComputeSH"), ShaderProperties::Merge(shaderProperties, { { "MODE_FINALIZE" } })), ComputePipelineRef() } }
    };

    ShaderRef firstShader;

    for (auto& it : pipelines)
    {
        Assert(it.second.first.IsValid());

        if (!firstShader)
        {
            firstShader = it.second.first;
        }
    }

    const DescriptorTableDeclaration& descriptorTableDecl = firstShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    Array<DescriptorTableRef> computeShDescriptorTables;
    computeShDescriptorTables.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        computeShDescriptorTables[i] = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& computeShDescriptorSet = computeShDescriptorTables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frameIndex);
            Assert(computeShDescriptorSet != nullptr);

            computeShDescriptorSet->SetElement(NAME("InColorCubemap"), colorAttachment->GetImageView());
            computeShDescriptorSet->SetElement(NAME("InNormalsCubemap"), normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement(NAME("InDepthCubemap"), depthAttachment ? depthAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
            computeShDescriptorSet->SetElement(NAME("InputSHTilesBuffer"), shTilesBuffers[i]);

            if (i != shNumLevels - 1)
            {
                computeShDescriptorSet->SetElement(NAME("OutputSHTilesBuffer"), shTilesBuffers[i + 1]);
            }
            else
            {
                computeShDescriptorSet->SetElement(NAME("OutputSHTilesBuffer"), shTilesBuffers[i]);
            }
        }

        DeferCreate(computeShDescriptorTables[i]);
    }

    for (auto& it : pipelines)
    {
        ComputePipelineRef& pipeline = it.second.second;

        pipeline = g_renderBackend->MakeComputePipeline(
            it.second.first,
            computeShDescriptorTables[0]);

        HYPERION_ASSERT_RESULT(pipeline->Create());
    }

    // Bind a directional light and sky envprobe if available
    EnvProbe* skyProbe = nullptr;
    Light* directionalLight = nullptr;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LT_DIRECTIONAL)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(light) != ~0u, "Light not bound!");

            directionalLight = light;

            break;
        }
    }

    if (const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
    {
        skyProbe = skyProbes.Front();
        AssertDebug(skyProbe != nullptr);
        AssertDebug(skyProbe->IsA<SkyProbe>());
    }

    const Vec2u cubemapDimensions = colorAttachment->GetImage()->GetExtent().GetXY();

    struct
    {
        Vec4u probeGridPosition;
        Vec4u cubemapDimensions;
        Vec4u levelDimensions;
        Vec4f worldPosition;
        uint32 envProbeIndex;
    } pushConstants;

    pushConstants.envProbeIndex = RenderApi_RetrieveResourceBinding(envProbe);
    pushConstants.probeGridPosition = { 0, 0, 0, 0 };
    pushConstants.cubemapDimensions = Vec4u { cubemapDimensions, 0, 0 };
    pushConstants.worldPosition = envProbe->GetRenderResource().GetBufferData().worldPosition;

    AssertDebug(pushConstants.envProbeIndex != ~0u);

    pipelines[NAME("Clear")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));
    pipelines[NAME("BuildCoeffs")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    RenderQueue& asyncComputeCommandList = g_renderBackend->GetAsyncCompute()->renderQueue;

    asyncComputeCommandList << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncComputeCommandList << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[NAME("Clear")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pipelines[NAME("Clear")].second);
    asyncComputeCommandList << DispatchCompute(pipelines[NAME("Clear")].second, Vec3u { 1, 1, 1 });

    asyncComputeCommandList << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncComputeCommandList << BindDescriptorTable(
        computeShDescriptorTables[0],
        pipelines[NAME("BuildCoeffs")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pipelines[NAME("BuildCoeffs")].second);
    asyncComputeCommandList << DispatchCompute(pipelines[NAME("BuildCoeffs")].second, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (shParallelReduce)
    {
        for (uint32 i = 1; i < shNumLevels; i++)
        {
            asyncComputeCommandList << InsertBarrier(
                shTilesBuffers[i - 1],
                RS_UNORDERED_ACCESS,
                SMT_COMPUTE);

            const Vec2u prevDimensions {
                MathUtil::Max(1u, shNumSamples.x >> (i - 1)),
                MathUtil::Max(1u, shNumSamples.y >> (i - 1))
            };

            const Vec2u nextDimensions {
                MathUtil::Max(1u, shNumSamples.x >> i),
                MathUtil::Max(1u, shNumSamples.y >> i)
            };

            Assert(prevDimensions.x >= 2);
            Assert(prevDimensions.x > nextDimensions.x);
            Assert(prevDimensions.y > nextDimensions.y);

            pushConstants.levelDimensions = {
                prevDimensions.x,
                prevDimensions.y,
                nextDimensions.x,
                nextDimensions.y
            };

            pipelines[NAME("Reduce")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

            asyncComputeCommandList << BindDescriptorTable(
                computeShDescriptorTables[i - 1],
                pipelines[NAME("Reduce")].second,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
                frame->GetFrameIndex());

            asyncComputeCommandList << BindComputePipeline(pipelines[NAME("Reduce")].second);
            asyncComputeCommandList << DispatchCompute(pipelines[NAME("Reduce")].second, Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 });
        }
    }

    const uint32 finalizeShBufferIndex = shParallelReduce ? shNumLevels - 1 : 0;

    // Finalize - build into final buffer
    asyncComputeCommandList << InsertBarrier(shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    pipelines[NAME("Finalize")].second->SetPushConstants(&pushConstants, sizeof(pushConstants));

    asyncComputeCommandList << BindDescriptorTable(
        computeShDescriptorTables[finalizeShBufferIndex],
        pipelines[NAME("Finalize")].second,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(directionalLight, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(skyProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pipelines[NAME("Finalize")].second);
    asyncComputeCommandList << DispatchCompute(pipelines[NAME("Finalize")].second, Vec3u { 1, 1, 1 });

    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind([renderEnvProbe = TResourceHandle<RenderEnvProbe>(envProbe->GetRenderResource()), pipelines = std::move(pipelines), descriptorTables = std::move(computeShDescriptorTables), delegateHandle](FrameBase* frame) mutable
        {
            HYP_NAMED_SCOPE("EnvProbe::ComputeSH - Buffer readback");

            const uint32 boundIndex = RenderApi_RetrieveResourceBinding(renderEnvProbe->GetEnvProbe());
            Assert(boundIndex != ~0u);

            EnvProbeShaderData readbackBuffer;

            g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->ReadbackElement(frame->GetFrameIndex(), boundIndex, &readbackBuffer);

            Memory::MemCpy(renderEnvProbe->m_sphericalHarmonics.values, readbackBuffer.sh.values, sizeof(EnvProbeSphericalHarmonics::values));

            HYP_LOG(EnvProbe, Debug, "EnvProbe {} (type: {}) SH computed", renderEnvProbe->GetEnvProbe()->Id(), renderEnvProbe->GetEnvProbe()->GetEnvProbeType());
            for (uint32 i = 0; i < 9; i++)
            {
                HYP_LOG(EnvProbe, Debug, "SH[{}]: {}", i, renderEnvProbe->m_sphericalHarmonics.values[i]);
            }

            renderEnvProbe->SetNeedsUpdate();

            for (auto& it : pipelines)
            {
                ShaderRef& shader = it.second.first;
                ComputePipelineRef& pipeline = it.second.second;

                SafeRelease(std::move(shader));
                SafeRelease(std::move(pipeline));
            }

            SafeRelease(std::move(descriptorTables));

            delete delegateHandle;
        });
}

#pragma endregion ReflectionProbeRenderer

} // namespace hyperion
