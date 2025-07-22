/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderShadowMap.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <rendering/Texture.hpp>
#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#if 0

static const TextureFormat shadowMapFormats[uint32(SMF_MAX)] = {
    TF_R32F, // STANDARD
    TF_R32F, // PCF
    TF_R32F, // CONTACT_HARDENED
    TF_RG32F // VSM
};

#pragma region ShadowPass

ShadowPass::ShadowPass(
    const Handle<Scene>& parentScene,
    const TResourceHandle<RenderWorld>& worldResourceHandle,
    const TResourceHandle<RenderCamera>& cameraResourceHandle,
    const TResourceHandle<RenderShadowMap>& shadowMapResourceHandle,
    const TResourceHandle<RenderView>& viewStaticsResourceHandle,
    const TResourceHandle<RenderView>& viewDynamicsResourceHandle,
    const ShaderRef& shader,
    RerenderShadowsSemaphore* rerenderSemaphore)
    : FullScreenPass(shadowMapFormats[uint32(shadowMapResourceHandle->GetFilterMode())], shadowMapResourceHandle->GetExtent(), nullptr),
      m_parentScene(parentScene),
      m_worldResourceHandle(worldResourceHandle),
      m_cameraResourceHandle(cameraResourceHandle),
      m_shadowMapResourceHandle(shadowMapResourceHandle),
      m_renderViewStatics(viewStaticsResourceHandle),
      m_renderViewDynamics(viewDynamicsResourceHandle),
      m_rerenderSemaphore(rerenderSemaphore)
{
    Assert(m_rerenderSemaphore != nullptr);

    SetShader(shader);
}

ShadowPass::~ShadowPass()
{
    m_parentScene.Reset();
    m_shadowMapStatics.Reset();
    m_shadowMapDynamics.Reset();

    SafeRelease(std::move(m_blurShadowMapPipeline));
}

void ShadowPass::CreateFramebuffer()
{
    m_framebuffer = g_renderBackend->MakeFramebuffer(GetExtent());

    // depth, depth^2 texture (for variance shadow map)
    AttachmentRef momentsAttachment = m_framebuffer->AddAttachment(
        0,
        GetFormat(),
        TT_TEX2D,
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    momentsAttachment->SetClearColor(MathUtil::Infinity<Vec4f>());

    // standard depth texture
    m_framebuffer->AddAttachment(
        1,
        g_renderBackend->GetDefaultFormat(DIF_DEPTH),
        TT_TEX2D,
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    DeferCreate(m_framebuffer);
}

void ShadowPass::CreateShadowMap()
{
    Assert(m_worldResourceHandle);

    const ShadowMapAtlasElement& atlasElement = m_shadowMapResourceHandle->GetAtlasElement();
    Assert(atlasElement.atlasIndex != ~0u);

    FixedArray<Handle<Texture>*, 2> shadowMapTextures {
        &m_shadowMapStatics,
        &m_shadowMapDynamics
    };

    for (Handle<Texture>* texturePtr : shadowMapTextures)
    {
        Handle<Texture>& texture = *texturePtr;

        texture = CreateObject<Texture>(TextureDesc {
            TT_TEX2D,
            GetFormat(),
            Vec3u { GetExtent().x, GetExtent().y, 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED });

        InitObject(texture);

        texture->SetPersistentRenderResourceEnabled(true);
    }
}

void ShadowPass::CreateCombineShadowMapsPass()
{
    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("CombineShadowMaps"), { { "STAGE_DYNAMICS" } });
    Assert(shader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet(NAME("CombineShadowMapsDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("PrevTexture"), m_shadowMapStatics->GetRenderResource().GetImageView());
        descriptorSet->SetElement(NAME("InTexture"), m_shadowMapDynamics->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptorTable);

    m_combineShadowMapsPass = MakeUnique<FullScreenPass>(shader, descriptorTable, GetFormat(), GetExtent(), m_gbuffer);
    m_combineShadowMapsPass->Create();
}

void ShadowPass::CreateComputePipelines()
{
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

        descriptorSet->SetElement(NAME("InputTexture"), m_framebuffer->GetAttachment(0)->GetImageView());
        descriptorSet->SetElement(NAME("OutputTexture"), m_shadowMapResourceHandle->GetImageView());
    }

    DeferCreate(descriptorTable);

    m_blurShadowMapPipeline = g_renderBackend->MakeComputePipeline(
        blurShadowMapShader,
        descriptorTable);

    DeferCreate(m_blurShadowMapPipeline);
}

void ShadowPass::Create()
{
    CreateShadowMap();
    CreateFramebuffer();
    CreateCombineShadowMapsPass();
    CreateComputePipelines();
}

void ShadowPass::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);

    if (!m_cameraResourceHandle)
    {
        return;
    }

    const ImageRef& framebufferImage = GetFramebuffer()->GetAttachment(0)->GetImage();

    if (framebufferImage == nullptr)
    {
        return;
    }

    Assert(m_parentScene.IsValid());

    const RenderSetup renderSetupStatics { renderSetup.world, m_renderViewStatics.Get() };
    const RenderSetup renderSetupDynamics { renderSetup.world, m_renderViewDynamics.Get() };

    { // Render each shadow map as needed
        if (m_rerenderSemaphore->IsInSignalState())
        {
            HYP_LOG(Shadows, Debug, "Rerendering static objects for shadow map");

            RenderCollector::ExecuteDrawCalls(
                frame,
                renderSetupStatics,
                RenderApi_GetConsumerProxyList(m_renderViewStatics->GetView()),
                ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

            // copy static framebuffer image
            frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
            frame->renderQueue << InsertBarrier(m_shadowMapStatics->GetRenderResource().GetImage(), RS_COPY_DST);

            frame->renderQueue << Blit(framebufferImage, m_shadowMapStatics->GetRenderResource().GetImage());

            frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
            frame->renderQueue << InsertBarrier(m_shadowMapStatics->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);

            m_rerenderSemaphore->Release(1);
        }

        { // Render dynamics
            RenderCollector::ExecuteDrawCalls(
                frame,
                renderSetupDynamics,
                RenderApi_GetConsumerProxyList(m_renderViewDynamics->GetView()),
                ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

            // copy dynamic framebuffer image
            frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
            frame->renderQueue << InsertBarrier(m_shadowMapDynamics->GetRenderResource().GetImage(), RS_COPY_DST);

            frame->renderQueue << Blit(framebufferImage, m_shadowMapDynamics->GetRenderResource().GetImage());

            frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
            frame->renderQueue << InsertBarrier(m_shadowMapDynamics->GetRenderResource().GetImage(), RS_SHADER_RESOURCE);
        }
    }

    const ShadowMapAtlasElement& atlasElement = m_shadowMapResourceHandle->GetAtlasElement();

    const ImageViewRef& shadowMapImageView = m_shadowMapResourceHandle->GetImageView();
    Assert(shadowMapImageView != nullptr);

    const ImageRef& shadowMapImage = shadowMapImageView->GetImage();
    Assert(shadowMapImage != nullptr);

    { // Combine static and dynamic shadow maps
        AttachmentBase* attachment = m_combineShadowMapsPass->GetFramebuffer()->GetAttachment(0);
        Assert(attachment != nullptr);

        m_combineShadowMapsPass->Render(frame, renderSetupStatics);

        // Copy combined shadow map to the final shadow map
        frame->renderQueue << InsertBarrier(attachment->GetImage(), RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(shadowMapImage, RS_COPY_DST, ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });

        // copy the image
        frame->renderQueue << Blit(
            attachment->GetImage(),
            shadowMapImage,
            Rect<uint32> { 0, 0, GetExtent().x, GetExtent().y },
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
        frame->renderQueue << InsertBarrier(attachment->GetImage(), RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(
            shadowMapImage,
            RS_SHADER_RESOURCE,
            ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });
    }

    if (m_shadowMapResourceHandle->GetFilterMode() == SMF_VSM)
    {
        struct
        {
            Vec2u imageDimensions;
            Vec2u dimensions;
            Vec2u offset;
        } pushConstants;

        pushConstants.imageDimensions = shadowMapImage->GetExtent().GetXY();
        pushConstants.dimensions = atlasElement.dimensions;
        pushConstants.offset = atlasElement.offsetCoords;

        m_blurShadowMapPipeline->SetPushConstants(&pushConstants, sizeof(pushConstants));

        // blur the image using compute shader
        frame->renderQueue << BindComputePipeline(m_blurShadowMapPipeline);

        // bind descriptor set containing info needed to blur
        frame->renderQueue << BindDescriptorTable(
            m_blurShadowMapPipeline->GetDescriptorTable(),
            m_blurShadowMapPipeline,
            ArrayMap<Name, ArrayMap<Name, uint32>> {},
            frame->GetFrameIndex());

        // put our shadow map in a state for writing
        frame->renderQueue << InsertBarrier(
            shadowMapImage,
            RS_UNORDERED_ACCESS,
            ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });

        frame->renderQueue << DispatchCompute(
            m_blurShadowMapPipeline,
            Vec3u {
                (atlasElement.dimensions.x + 7) / 8,
                (atlasElement.dimensions.y + 7) / 8,
                1 });

        // put shadow map back into readable state
        frame->renderQueue << InsertBarrier(
            shadowMapImage,
            RS_SHADER_RESOURCE,
            ImageSubResource { .baseArrayLayer = atlasElement.atlasIndex });
    }
}

#pragma endregion ShadowPass

#pragma region DirectionalLightShadowRenderer

DirectionalLightShadowRenderer::DirectionalLightShadowRenderer(const Handle<Scene>& parentScene, const Handle<Light>& light, Vec2u resolution, ShadowMapFilter filterMode)
    : m_parentScene(parentScene),
      m_light(light),
      m_resolution(resolution),
      m_filterMode(filterMode)
{
    m_camera = CreateObject<Camera>(m_resolution.x, m_resolution.y);
    m_camera->SetName(NAME("DirectionalLightShadowRendererCamera"));
    m_camera->AddCameraController(CreateObject<OrthoCameraController>());

    InitObject(m_camera);

    const RenderableAttributeSet overrideAttributes(
        MeshAttributes {},
        MaterialAttributes {
            .shaderDefinition = m_shadowPass->GetShader()->GetCompiledShader()->GetDefinition(),
            .cullFaces = m_shadowMapResourceHandle->GetFilterMode() == SMF_VSM ? FCM_BACK : FCM_FRONT });

    m_viewStatics = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::COLLECT_STATIC_ENTITIES,
        .viewport = Viewport { .extent = m_resolution, .position = Vec2i::Zero() },
        .scenes = { m_parentScene },
        .camera = m_camera,
        .overrideAttributes = overrideAttributes });

    InitObject(m_viewStatics);

    m_viewDynamics = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::COLLECT_DYNAMIC_ENTITIES,
        .viewport = Viewport { .extent = m_resolution, .position = Vec2i::Zero() },
        .scenes = { m_parentScene },
        .camera = m_camera,
        .overrideAttributes = overrideAttributes });

    InitObject(m_viewDynamics);

    CreateShader();
}

DirectionalLightShadowRenderer::~DirectionalLightShadowRenderer()
{
    HYP_SYNC_RENDER(); // wait for render commands to finish

    m_shadowPass.Reset();
}

void DirectionalLightShadowRenderer::Init()
{
    SetReady(true);
}

void DirectionalLightShadowRenderer::OnAddedToWorld()
{
    return;

    RenderShadowMap* shadowMap = g_renderGlobalState->shadowMapAllocator->AllocateShadowMap(SMT_DIRECTIONAL, m_filterMode, m_resolution);
    Assert(shadowMap != nullptr, "Failed to allocate shadow map");

    m_shadowMapResourceHandle = TResourceHandle<RenderShadowMap>(*shadowMap);

    if (InitObject(m_light))
    {
        // m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>(m_shadowMapResourceHandle));
    }

    m_shadowPass = MakeUnique<ShadowPass>(
        m_parentScene,
        TResourceHandle<RenderWorld>(m_parentScene->GetWorld()->GetRenderResource()),
        TResourceHandle<RenderCamera>(m_camera->GetRenderResource()),
        m_shadowMapResourceHandle,
        TResourceHandle<RenderView>(m_viewStatics->GetRenderResource()),
        TResourceHandle<RenderView>(m_viewDynamics->GetRenderResource()),
        m_shader,
        &m_rerenderSemaphore);

    m_shadowPass->Create();
}

void DirectionalLightShadowRenderer::OnRemovedFromWorld()
{
    if (m_light.IsValid())
    {
        // m_light->GetRenderResource().SetShadowMap(TResourceHandle<RenderShadowMap>());
    }

    m_shadowPass.Reset();
    m_camera.Reset();

    if (m_shadowMapResourceHandle)
    {
        RenderShadowMap* shadowMap = m_shadowMapResourceHandle.Get();

        m_shadowMapResourceHandle.Reset();

        if (!g_renderGlobalState->shadowMapAllocator->FreeShadowMap(shadowMap))
        {
            HYP_LOG(Shadows, Error, "Failed to free shadow map!");
        }
    }
}

void DirectionalLightShadowRenderer::Update(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);

    struct RENDER_COMMAND(RenderShadowPass)
        : public RenderCommand
    {
        RenderWorld* world;
        ShadowPass* shadowPass;

        RENDER_COMMAND(RenderShadowPass)(RenderWorld* world, ShadowPass* shadowPass)
            : world(world),
              shadowPass(shadowPass)
        {
        }

        virtual ~RENDER_COMMAND(RenderShadowPass)() override = default;

        virtual RendererResult operator()() override
        {
            FrameBase* frame = g_renderBackend->GetCurrentFrame();

            RenderSetup renderSetup { world, nullptr };

            shadowPass->Render(frame, renderSetup);

            HYPERION_RETURN_OK;
        }
    };

    Assert(m_camera != nullptr);
    m_camera->Update(delta);

    m_viewStatics->UpdateVisibility();
    m_viewStatics->Collect();

    m_viewDynamics->UpdateVisibility();
    m_viewDynamics->Collect();

    Octree& octree = m_parentScene->GetOctree();

    auto staticsCollectionResult = m_viewStatics->GetLastMeshCollectionResult();

    Octree const* fittingOctant = nullptr;
    octree.GetFittingOctant(m_aabb, fittingOctant);

    if (!fittingOctant)
    {
        fittingOctant = &octree;
    }

    const HashCode octantHashStatics = fittingOctant->GetOctantID().GetHashCode().Add(fittingOctant->GetEntryListHash<EntityTag::STATIC>()).Add(fittingOctant->GetEntryListHash<EntityTag::LIGHT>());

    // Need to re-render static objects if:
    // * octant's statics hash code has changed
    // * camera view has changed
    // * static objects have been added, removed or changed
    bool needsStaticsRerender = false;
    needsStaticsRerender |= m_cachedViewMatrix != m_camera->GetViewMatrix();
    needsStaticsRerender |= m_cachedOctantHashCodeStatics != octantHashStatics;
    needsStaticsRerender |= staticsCollectionResult.NeedsUpdate();

    if (needsStaticsRerender)
    {
        m_cachedViewMatrix = m_camera->GetViewMatrix();

        // Force static objects to re-render for a few frames
        m_rerenderSemaphore.Produce(1);

        m_cachedViewMatrix = m_camera->GetViewMatrix();
        m_cachedOctantHashCodeStatics = octantHashStatics;
    }

    m_shadowMapResourceHandle->SetBufferData(ShadowMapShaderData {
        .projection = m_camera->GetProjectionMatrix(),
        .view = m_camera->GetViewMatrix(),
        .aabbMax = Vec4f(m_aabb.max, 1.0f),
        .aabbMin = Vec4f(m_aabb.min, 1.0f) });

    PUSH_RENDER_COMMAND(RenderShadowPass, &g_engine->GetWorld()->GetRenderResource(), m_shadowPass.Get());
}

void DirectionalLightShadowRenderer::CreateShader()
{
    ShaderProperties properties;
    properties.SetRequiredVertexAttributes(staticMeshVertexAttributes);

    switch (m_filterMode)
    {
    case SMF_VSM:
        properties.Set("MODE_VSM");
        break;
    case SMF_CONTACT_HARDENED:
        properties.Set("MODE_CONTACT_HARDENED");
        break;
    case SMF_PCF:
        properties.Set("MODE_PCF");
        break;
    case SMF_STANDARD: // fallthrough
    default:
        properties.Set("MODE_STANDARD");
        break;
    }

    m_shader = g_shaderManager->GetOrCreate(
        NAME("Shadows"),
        properties);
}

#pragma endregion DirectionalLightShadowRenderer

#endif

} // namespace hyperion
