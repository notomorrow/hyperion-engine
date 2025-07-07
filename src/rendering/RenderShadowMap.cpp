/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/FullScreenPass.hpp>

#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region ShadowMapAtlas

bool ShadowMapAtlas::AddElement(const Vec2u& elementDimensions, ShadowMapAtlasElement& outElement)
{
    if (!AtlasPacker<ShadowMapAtlasElement>::AddElement(elementDimensions, outElement))
    {
        return false;
    }

    outElement.atlasIndex = atlasIndex;

    return true;
}

#pragma endregion ShadowMapAtlas

#pragma region ShadowMapAllocator

ShadowMapAllocator::ShadowMapAllocator()
    : m_atlasDimensions(2048, 2048)
{
    m_atlases.Reserve(maxShadowMaps);

    for (SizeType i = 0; i < maxShadowMaps; i++)
    {
        m_atlases.PushBack(ShadowMapAtlas(uint32(i), m_atlasDimensions));
    }
}

ShadowMapAllocator::~ShadowMapAllocator()
{
    SafeRelease(std::move(m_atlasImage));
    SafeRelease(std::move(m_atlasImageView));

    SafeRelease(std::move(m_pointLightShadowMapImage));
    SafeRelease(std::move(m_pointLightShadowMapImageView));
}

void ShadowMapAllocator::Initialize()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    m_atlasImage = g_renderBackend->MakeImage(TextureDesc {
        TT_TEX2D_ARRAY,
        TF_RG32F,
        Vec3u { m_atlasDimensions, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        uint32(m_atlases.Size()),
        IU_SAMPLED | IU_STORAGE });

    HYPERION_ASSERT_RESULT(m_atlasImage->Create());

    m_atlasImageView = g_renderBackend->MakeImageView(m_atlasImage);
    HYPERION_ASSERT_RESULT(m_atlasImageView->Create());

    m_pointLightShadowMapImage = g_renderBackend->MakeImage(TextureDesc {
        TT_CUBEMAP_ARRAY,
        TF_RG32F,
        Vec3u { 512, 512, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        g_maxBoundPointShadowMaps * 6,
        IU_SAMPLED | IU_STORAGE });

    HYPERION_ASSERT_RESULT(m_pointLightShadowMapImage->Create());

    m_pointLightShadowMapImageView = g_renderBackend->MakeImageView(m_pointLightShadowMapImage);
    HYPERION_ASSERT_RESULT(m_pointLightShadowMapImageView->Create());
}

void ShadowMapAllocator::Destroy()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    SafeRelease(std::move(m_atlasImage));
    SafeRelease(std::move(m_atlasImageView));

    SafeRelease(std::move(m_pointLightShadowMapImage));
    SafeRelease(std::move(m_pointLightShadowMapImageView));
}

RenderShadowMap* ShadowMapAllocator::AllocateShadowMap(ShadowMapType shadowMapType, ShadowMapFilter filterMode, const Vec2u& dimensions)
{
    if (shadowMapType == SMT_OMNI)
    {
        const uint32 pointLightIndex = m_pointLightShadowMapIdGenerator.Next() - 1;

        // Cannot allocate if we ran out of IDs
        if (pointLightIndex >= g_maxBoundPointShadowMaps)
        {
            m_pointLightShadowMapIdGenerator.ReleaseId(pointLightIndex + 1);

            return nullptr;
        }

        const ShadowMapAtlasElement atlasElement {
            .atlasIndex = ~0u,
            .pointLightIndex = pointLightIndex,
            .offsetUv = Vec2f::Zero(),
            .offsetCoords = Vec2u::Zero(),
            .dimensions = dimensions,
            .scale = Vec2f::One()
        };

        RenderShadowMap* shadowMap = AllocateResource<RenderShadowMap>(
            shadowMapType,
            filterMode,
            atlasElement,
            m_pointLightShadowMapImageView);

        return shadowMap;
    }

    for (ShadowMapAtlas& atlas : m_atlases)
    {
        ShadowMapAtlasElement atlasElement;

        if (atlas.AddElement(dimensions, atlasElement))
        {
            ImageViewRef atlasImageView = m_atlasImage->MakeLayerImageView(atlasElement.atlasIndex);
            DeferCreate(atlasImageView);

            RenderShadowMap* shadowMap = AllocateResource<RenderShadowMap>(
                shadowMapType,
                filterMode,
                atlasElement,
                atlasImageView);

            return shadowMap;
        }
    }

    return nullptr;
}

bool ShadowMapAllocator::FreeShadowMap(RenderShadowMap* shadowMap)
{
    if (!shadowMap)
    {
        return false;
    }

    const ShadowMapAtlasElement& atlasElement = shadowMap->GetAtlasElement();

    bool result = false;

    if (atlasElement.atlasIndex != ~0u)
    {
        Assert(atlasElement.atlasIndex < m_atlases.Size());

        ShadowMapAtlas& atlas = m_atlases[atlasElement.atlasIndex];
        result = atlas.RemoveElement(atlasElement);

        if (!result)
        {
            HYP_LOG(Rendering, Error, "Failed to free shadow map from atlas (atlas index: {})", atlasElement.atlasIndex);
        }
    }
    else if (atlasElement.pointLightIndex != ~0u)
    {
        m_pointLightShadowMapIdGenerator.ReleaseId(atlasElement.pointLightIndex + 1);

        result = true;
    }
    else
    {
        HYP_LOG(Rendering, Error, "Failed to free shadow map: invalid atlas index and point light index");
    }

    FreeResource(shadowMap);

    return result;
}

#pragma endregion ShadowMapAllocator

#pragma region RenderShadowMap

RenderShadowMap::RenderShadowMap(ShadowMapType type, ShadowMapFilter filterMode, const ShadowMapAtlasElement& atlasElement, const ImageViewRef& imageView)
    : m_type(type),
      m_filterMode(filterMode),
      m_atlasElement(atlasElement),
      m_imageView(imageView),
      m_bufferData {}
{
    HYP_LOG(Rendering, Debug, "Creating shadow map for atlas element, (atlas: {}, offset: {}, dimensions: {}, scale: {})",
        atlasElement.atlasIndex,
        atlasElement.offsetCoords,
        atlasElement.dimensions,
        atlasElement.scale);
}

RenderShadowMap::RenderShadowMap(RenderShadowMap&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_type(other.m_type),
      m_filterMode(other.m_filterMode),
      m_atlasElement(other.m_atlasElement),
      m_imageView(std::move(other.m_imageView)),
      m_bufferData(std::move(other.m_bufferData))
{
}

RenderShadowMap::~RenderShadowMap()
{
    SafeRelease(std::move(m_imageView));
}

void RenderShadowMap::Initialize_Internal()
{
    HYP_SCOPE;

    UpdateBufferData();
}

void RenderShadowMap::Destroy_Internal()
{
    HYP_SCOPE;
}

void RenderShadowMap::Update_Internal()
{
    HYP_SCOPE;
}

GpuBufferHolderBase* RenderShadowMap::GetGpuBufferHolder() const
{
    return g_renderGlobalState->gpuBuffers[GRB_SHADOW_MAPS];
}

void RenderShadowMap::SetBufferData(const ShadowMapShaderData& bufferData)
{
    HYP_SCOPE;

    Execute([this, bufferData]()
        {
            m_bufferData = bufferData;

            if (IsInitialized())
            {
                UpdateBufferData();
            }
        });
}

void RenderShadowMap::UpdateBufferData()
{
    HYP_SCOPE;

    Assert(m_bufferIndex != ~0u);

    m_bufferData.dimensionsScale = Vec4f(Vec2f(m_atlasElement.dimensions), m_atlasElement.scale);
    m_bufferData.offsetUv = m_atlasElement.offsetUv;
    m_bufferData.layerIndex = m_type == SMT_OMNI ? m_atlasElement.pointLightIndex : m_atlasElement.atlasIndex;
    m_bufferData.flags = uint32(SF_NONE);

    switch (m_filterMode)
    {
    case SMF_VSM:
        m_bufferData.flags |= uint32(SF_VSM);
        break;
    case SMF_CONTACT_HARDENED:
        m_bufferData.flags |= uint32(SF_CONTACT_HARDENED);
        break;
    case SMF_PCF:
        m_bufferData.flags |= uint32(SF_PCF);
        break;
    default:
        break;
    }

    *static_cast<ShadowMapShaderData*>(m_bufferAddress) = m_bufferData;
    GetGpuBufferHolder()->MarkDirty(m_bufferIndex);
}

#pragma endregion RenderShadowMap

#pragma region ShadowPassData

ShadowPassData::~ShadowPassData()
{
}

#pragma endregion ShadowPassData

#pragma region ShadowRendererBase

static UniquePtr<FullScreenPass> CreateCombineShadowMapsPass(TextureFormat format, Vec2u dimensions, Span<Handle<View>> views)
{
    AssertDebug(views.Size() >= 2, "Combine pass requires 2 views (one for static objects, one for dynamic objects)");

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("CombineShadowMaps"), { { "STAGE_DYNAMICS" } });
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("CombineShadowMapsDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("PrevTexture"), views[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
        descriptorSet->SetElement(NAME("InTexture"), views[1]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView());
    }

    DeferCreate(descriptorTable);

    UniquePtr<FullScreenPass> combineShadowMapsPass = MakeUnique<FullScreenPass>(
        shader,
        descriptorTable,
        format,
        dimensions,
        nullptr);

    combineShadowMapsPass->Create();

    return combineShadowMapsPass;
}

static ComputePipelineRef CreateBlurShadowMapPipeline(const ImageViewRef& input, const ImageViewRef& output)
{
    Assert(input.IsValid());
    Assert(output.IsValid());

    ShaderRef blurShadowMapShader = g_shaderManager->GetOrCreate(NAME("BlurShadowMap"));
    Assert(blurShadowMapShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = blurShadowMapShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("BlurShadowMapDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InputTexture"), input);
        descriptorSet->SetElement(NAME("OutputTexture"), output);
    }

    DeferCreate(descriptorTable);

    ComputePipelineRef blurShadowMapPipeline = g_renderBackend->MakeComputePipeline(
        blurShadowMapShader,
        descriptorTable);

    DeferCreate(blurShadowMapPipeline);

    return blurShadowMapPipeline;
}

ShadowRendererBase::ShadowRendererBase() = default;

void ShadowRendererBase::Initialize()
{
}

void ShadowRendererBase::Shutdown()
{
    HashSet<RenderShadowMap*> shadowMaps;

    for (const KeyValuePair<WeakHandle<Light>, CachedShadowMapData>& it : m_cachedShadowMapData)
    {
        if (!it.second.shadowMap)
        {
            continue;
        }

        shadowMaps.Insert(it.second.shadowMap);
    }

    m_cachedShadowMapData.Clear();

    if (shadowMaps.Any())
    {
        for (RenderShadowMap* shadowMap : shadowMaps)
        {
            bool shadowMapFreed = g_renderGlobalState->shadowMapAllocator->FreeShadowMap(shadowMap);
            AssertDebug(shadowMapFreed, "Failed to free shadow map");
        }
    }
}

int ShadowRendererBase::RunCleanupCycle(int maxIter)
{
    int numCycles = RendererBase::RunCleanupCycle(maxIter);

    for (auto it = m_cachedShadowMapData.Begin(); it != m_cachedShadowMapData.End() && numCycles < maxIter; numCycles++)
    {
        if (!(it->first.Lock()))
        {
            HYP_LOG(Rendering, Debug, "Removing cached shadow map for Light {} as it is no longer valid.", it->first.Id());

            if (it->second.shadowMap != nullptr)
            {
                bool shadowMapFreed = g_renderGlobalState->shadowMapAllocator->FreeShadowMap(it->second.shadowMap);
                AssertDebug(shadowMapFreed, "Failed to free shadow map for Light {}!", it->first.Id());
            }

            it = m_cachedShadowMapData.Erase(it);

            continue;
        }

        ++it;
    }

    return numCycles;
}

void ShadowRendererBase::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.light != nullptr);

    Light* light = renderSetup.light;
    RenderShadowMap* shadowMap = nullptr;

    RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(RenderApi_GetRenderProxy(light->Id()));
    Assert(lightProxy != nullptr, "Proxy for Light {} not found when rendering shadows!", light->Id());
    Assert(lightProxy->shadowViews.Any(), "Light {} proxy has no shadow view attached!", light->Id());

    Array<Handle<View>> shadowViews = Map(lightProxy->shadowViews, &WeakHandle<View>::Lock);

    // check views validity
    for (const Handle<View>& shadowView : shadowViews)
    {
        Assert(shadowView.IsValid());
        Assert(shadowView->GetOutputTarget().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer().IsValid());
        Assert(shadowView->GetOutputTarget().GetFramebuffer()->GetAttachment(0) != nullptr);
    }

    auto cacheIt = m_cachedShadowMapData.FindAs(light->Id());

    if (cacheIt == m_cachedShadowMapData.End())
    {
        WeakHandle<Light> lightWeak = light->WeakHandleFromThis();

        shadowMap = AllocateShadowMap(light);
        Assert(shadowMap != nullptr, "Failed to allocate shadow map for Light {}!", light->Id());

        cacheIt = m_cachedShadowMapData.Emplace(lightWeak).first;
        cacheIt->second.shadowMap = shadowMap;

        /// TODO: Better check for using combined pass
        if (lightProxy->shadowViews.Size() == 2)
        {
            cacheIt->second.combineShadowMapsPass = CreateCombineShadowMapsPass(
                shadowMap->GetImageView()->GetImage()->GetTextureFormat(), // @TODO get format from Light's settings
                shadowMap->GetExtent(),
                shadowViews);
        }

        if (shadowMap->GetFilterMode() == SMF_VSM)
        {
            const ImageViewRef& inputImageView = cacheIt->second.combineShadowMapsPass != nullptr
                ? cacheIt->second.combineShadowMapsPass->GetFinalImageView()
                : shadowViews[0]->GetOutputTarget().GetFramebuffer()->GetAttachment(0)->GetImageView();

            Assert(inputImageView.IsValid());

            /// TODO: Blur into separate texture before blitting to final shadow map,
            /// or we'll end up blurring other maps in the atlas multiple times!
            cacheIt->second.csBlurShadowMap = CreateBlurShadowMapPipeline(inputImageView, shadowMap->GetImageView());
        }

        /// TODO: Add re-alloc of shadow maps if parameters have changed
    }
    else
    {
        shadowMap = cacheIt->second.shadowMap;
        AssertDebug(shadowMap != nullptr);
    }

    const ShadowMapAtlasElement& atlasElement = shadowMap->GetAtlasElement();

    const ImageRef& shadowMapImage = shadowMap->GetImageView()->GetImage();
    Assert(shadowMapImage.IsValid());

    FullScreenPass* combineShadowMapsPass = cacheIt->second.combineShadowMapsPass.Get();
    const ComputePipelineRef& csBlurShadowMap = cacheIt->second.csBlurShadowMap;

    const bool useVsm = shadowMap->GetFilterMode() == SMF_VSM;

    Array<RenderProxyList*> renderProxyLists;
    renderProxyLists.Reserve(shadowViews.Size());

    HYP_DEFER({ for (RenderProxyList* rpl : renderProxyLists) rpl->EndRead(); });

    for (const Handle<View>& shadowView : shadowViews)
    {
        const ViewOutputTarget& outputTarget = shadowView->GetOutputTarget();
        Assert(outputTarget.IsValid());

        const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
        Assert(framebuffer.IsValid());

        RenderSetup rs = renderSetup;
        rs.view = &shadowView->GetRenderResource(); // temp
        rs.passData = FetchViewPassData(shadowView);

        ShadowPassData* pd = static_cast<ShadowPassData*>(rs.passData);
        AssertDebug(pd != nullptr);

        RenderProxyList& rpl = RenderApi_GetConsumerProxyList(shadowView);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        RenderCollector& renderCollector = RenderApi_GetRenderCollector(shadowView);
        HYP_LOG_TEMP("shadow render collector  {}", (void*)&renderCollector);

        HYP_LOG_TEMP("Render Shadow map here for light {}\t into view: {} for {} entities\t\tShadow map atlas index: {} elem index: {} point idx: {}",
            renderSetup.light->Id(),
            shadowView->Id(),
            renderCollector.NumDrawCallsCollected(),
            shadowMap->GetAtlasElement().atlasIndex, shadowMap->GetAtlasElement().index,
            shadowMap->GetAtlasElement().pointLightIndex);

        renderCollector.ExecuteDrawCalls(frame, rs, ((1u << RB_OPAQUE) | (1u << RB_LIGHTMAP) | (1u << RB_TRANSLUCENT)));

        if (!combineShadowMapsPass)
        {
            // blit image into final result
            const ImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();
            Assert(framebufferImage.IsValid());

            frame->GetCommandList().Add<InsertBarrier>(framebufferImage, RS_COPY_SRC);
            frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_COPY_DST);

            frame->GetCommandList().Add<Blit>(
                framebufferImage,
                shadowMapImage,
                Rect<uint32> { 0, 0, shadowMap->GetExtent().x, shadowMap->GetExtent().y },
                Rect<uint32> {
                    atlasElement.offsetCoords.x,
                    atlasElement.offsetCoords.y,
                    atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                    atlasElement.offsetCoords.y + atlasElement.dimensions.y },
                0,                      /* srcMip */
                0,                      /* dstMip */
                0,                      /* srcFace */
                atlasElement.atlasIndex /* dstFace */
            );

            frame->GetCommandList().Add<InsertBarrier>(framebufferImage, RS_SHADER_RESOURCE);
            frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_SHADER_RESOURCE);
        }
    }

    if (combineShadowMapsPass)
    {
        AttachmentBase* attachment = combineShadowMapsPass->GetFramebuffer()->GetAttachment(0);
        Assert(attachment != nullptr);

        RenderSetup rs = renderSetup;
        // FullScreenPass needs a View set
        rs.view = &shadowViews[0]->GetRenderResource();

        // Combine passes into one
        combineShadowMapsPass->Render(frame, rs);

        // Copy combined shadow map to the final shadow map
        frame->GetCommandList().Add<InsertBarrier>(attachment->GetImage(), RS_COPY_SRC);
        frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_COPY_DST, ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });

        // copy the image
        frame->GetCommandList().Add<Blit>(
            attachment->GetImage(),
            shadowMapImage,
            Rect<uint32> { 0, 0, shadowMap->GetExtent().x, shadowMap->GetExtent().y },
            Rect<uint32> {
                atlasElement.offsetCoords.x,
                atlasElement.offsetCoords.y,
                atlasElement.offsetCoords.x + atlasElement.dimensions.x,
                atlasElement.offsetCoords.y + atlasElement.dimensions.y },
            0,                      /* srcMip */
            0,                      /* dstMip */
            0,                      /* srcFace */
            atlasElement.atlasIndex /* dstFace */
        );

        // put the images back into a state for reading
        frame->GetCommandList().Add<InsertBarrier>(attachment->GetImage(), RS_SHADER_RESOURCE);
        frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_SHADER_RESOURCE, ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });
    }

    if (useVsm)
    {
        AssertDebug(csBlurShadowMap.IsValid());

        struct
        {
            Vec2u imageDimensions;
            Vec2u dimensions;
            Vec2u offset;
        } pushConstants;

        pushConstants.imageDimensions = shadowMapImage->GetExtent().GetXY();
        pushConstants.dimensions = atlasElement.dimensions;
        pushConstants.offset = atlasElement.offsetCoords;

        csBlurShadowMap->SetPushConstants(&pushConstants, sizeof(pushConstants));

        // blur the image using compute shader
        frame->GetCommandList().Add<BindComputePipeline>(csBlurShadowMap);

        // bind descriptor set containing info needed to blur
        frame->GetCommandList().Add<BindDescriptorTable>(csBlurShadowMap->GetDescriptorTable(), csBlurShadowMap, ArrayMap<Name, ArrayMap<Name, uint32>> {}, frame->GetFrameIndex());

        // put our shadow map in a state for writing
        frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_UNORDERED_ACCESS, ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });
        frame->GetCommandList().Add<DispatchCompute>(csBlurShadowMap, Vec3u { (atlasElement.dimensions.x + 7) / 8, (atlasElement.dimensions.y + 7) / 8, 1 });

        // put shadow map back into readable state
        frame->GetCommandList().Add<InsertBarrier>(shadowMapImage, RS_SHADER_RESOURCE, ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });
    }
}

PassData* ShadowRendererBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    ShadowPassData* pd = new ShadowPassData;
    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetRenderResource().GetViewport();

    return pd;
}

#pragma endregion ShadowRendererBase

#pragma region PointShadowRenderer

RenderShadowMap* PointShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderGlobalState->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_OMNI,
        light->GetShadowMapFilter(),
        Vec2u(256, 256)); /// TEMP! fixme
}

#pragma endregion PointShadowRenderer

#pragma region DirectionalShadowRenderer

RenderShadowMap* DirectionalShadowRenderer::AllocateShadowMap(Light* light)
{
    return g_renderGlobalState->shadowMapAllocator->AllocateShadowMap(
        ShadowMapType::SMT_DIRECTIONAL,
        light->GetShadowMapFilter(),
        Vec2u(256, 256)); /// TEMP! fixme
}

#pragma endregion DirectionalShadowRenderer

HYP_DESCRIPTOR_SRV(Global, ShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SRV(Global, PointLightShadowMapsTextureArray, 1);
HYP_DESCRIPTOR_SSBO(Global, ShadowMapsBuffer, 1, ~0u, false);

} // namespace hyperion