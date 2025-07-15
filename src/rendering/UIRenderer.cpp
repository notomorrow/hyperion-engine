/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderTexture.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

/// Includes needed for RenderCollection
#include <scene/Mesh.hpp>
#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(UI);

struct alignas(16) UIEntityInstanceBatch : EntityInstanceBatch
{
    Vec4f texcoords[maxEntitiesPerInstanceBatch];
    Vec4f offsets[maxEntitiesPerInstanceBatch];
    Vec4f sizes[maxEntitiesPerInstanceBatch];
};

static_assert(sizeof(UIEntityInstanceBatch) == 6976);

#pragma region UIPassData

struct UIPassData : PassData
{
};

#pragma endregion UIPassData

#pragma region Render commands

struct RENDER_COMMAND(AddUIRenderer)
    : RenderCommand
{
    UIRenderer* uiRenderer;

    RENDER_COMMAND(AddUIRenderer)(UIRenderer* uiRenderer)
        : uiRenderer(uiRenderer)
    {
        AssertDebug(uiRenderer != nullptr);
    }

    virtual RendererResult operator()() override
    {
        g_renderGlobalState->AddRenderer(GRT_UI, uiRenderer);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveUIRenderer)
    : RenderCommand
{
    UIRenderer* uiRenderer;

    RENDER_COMMAND(RemoveUIRenderer)(UIRenderer* uiRenderer)
        : uiRenderer(uiRenderer)
    {
    }

    virtual RendererResult operator()() override
    {
        g_renderGlobalState->RemoveRenderer(GRT_UI, uiRenderer);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(SetFinalPassImageView)
    : RenderCommand
{
    ImageViewRef imageView;

    RENDER_COMMAND(SetFinalPassImageView)(const ImageViewRef& imageView)
        : imageView(imageView)
    {
    }

    virtual ~RENDER_COMMAND(SetFinalPassImageView)() override = default;

    virtual RendererResult operator()() override
    {
        if (!imageView)
        {
            imageView = g_renderGlobalState->placeholderData->DefaultTexture2D->GetRenderResource().GetImageView();
        }

        g_engine->GetFinalPass()->SetUILayerImageView(imageView);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

static RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entityAttributes, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

    RenderableAttributeSet attributes = entityAttributes;

    if (overrideAttributes.HasValue())
    {
        if (const ShaderDefinition& overrideShaderDefinition = overrideAttributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(overrideShaderDefinition);
        }

        ShaderDefinition shaderDefinition = overrideAttributes->GetShaderDefinition().IsValid()
            ? overrideAttributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        Assert(shaderDefinition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet meshVertexAttributes = attributes.GetMeshAttributes().vertexAttributes;

        if (meshVertexAttributes != shaderDefinition.GetProperties().GetRequiredVertexAttributes())
        {
            shaderDefinition.properties.SetRequiredVertexAttributes(meshVertexAttributes);
        }

        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        newMaterialAttributes.shaderDefinition = shaderDefinition;
        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    return attributes;
}

static void BuildRenderGroups(RenderCollector& renderCollector, RenderProxyList& rpl, const Array<Pair<ObjId<Entity>, int>>& proxyDepths, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    renderCollector.Clear();

    for (const Pair<ObjId<Entity>, int>& pair : proxyDepths)
    {
        RenderProxyMesh* meshProxy = rpl.meshes.GetProxy(pair.first);
        AssertDebug(meshProxy != nullptr);

        if (!meshProxy)
        {
            continue;
        }

        const Handle<Mesh>& mesh = meshProxy->mesh;
        const Handle<Material>& material = meshProxy->material;

        if (!mesh.IsValid() || !material.IsValid())
        {
            continue;
        }

        RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet { mesh->GetMeshAttributes(), material->GetRenderAttributes() }, overrideAttributes);

        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        attributes.SetDrawableLayer(pair.second);

        DrawCallCollectionMapping& mapping = renderCollector.mappingsByBucket[rb][attributes];
        Handle<RenderGroup>& rg = mapping.renderGroup;

        if (!rg.IsValid())
        {
            ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();
            shaderDefinition.GetProperties().Set("INSTANCING");

            ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);
            Assert(shader.IsValid());

            rg = CreateObject<RenderGroup>(shader, attributes, RenderGroupFlags::NONE);

#ifdef HYP_DEBUG_MODE
            if (!rg.IsValid())
            {
                HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                continue;
            }
#endif

            InitObject(rg);
        }

        mapping.meshProxies.Set(meshProxy->entity.Id().ToIndex(), meshProxy);
    }
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(renderSetup.view->GetView());
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    // RenderCollector& renderCollector = RenderApi_GetRenderCollector(renderSetup.view->GetView());

    const uint32 frameIndex = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    {
        HYP_NAMED_SCOPE("Sort proxy groups by layer");

        std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
            {
                return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
            });
    }

    // HYP_LOG(UI, Debug, " UI rendering {} render groups", iterators.Size());

    for (SizeType index = 0; index < iterators.Size(); index++)
    {
        const auto& it = *iterators[index];

        const RenderableAttributeSet& attributes = it.first;
        const DrawCallCollectionMapping& mapping = it.second;
        Assert(mapping.IsValid());

        const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
        Assert(renderGroup.IsValid());

        const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

        // debugging
        for (const DrawCall& drawCall : drawCallCollection.drawCalls)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
        }
        for (const InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
        {
            AssertDebug(RenderApi_RetrieveResourceBinding(drawCall.material) != ~0u);
        }

        ParallelRenderingState* parallelRenderingState = nullptr;

        if (renderGroup->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
        {
            parallelRenderingState = AcquireNextParallelRenderingState();
        }

        // Don't count draw calls for UI
        SuppressRenderStatsScope suppressRenderStatsScope;

        renderGroup->PerformRendering(frame, renderSetup, drawCallCollection, nullptr, parallelRenderingState);

        if (parallelRenderingState != nullptr)
        {
            AssertDebug(parallelRenderingState->taskBatch != nullptr);

            TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
        }
    }

    // Wait for all parallel rendering tasks to finish
    CommitParallelRenderingState(frame->GetCommandList());

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderer

UIRenderer::UIRenderer(const Handle<View>& view)
    : m_view(view)
{
    AssertDebug(view.IsValid());
}

void UIRenderer::Initialize()
{
}

void UIRenderer::Shutdown()
{
}

void UIRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    UIPassData* pd = static_cast<UIPassData*>(FetchViewPassData(m_view));
    AssertDebug(pd != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = &m_view->GetRenderResource();
    rs.passData = pd;

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(m_view);
    rpl.BeginRead();

    // RenderCollector& renderCollector = RenderApi_GetRenderCollector(m_view);

    if (pd->viewport != m_view->GetRenderResource().GetViewport())
    {
        /// @TODO: Implement me!

        HYP_LOG(UI, Warning, "UIRenderer: Viewport size changed from {} to {}, resizing view pass data",
            pd->viewport.extent, m_view->GetRenderResource().GetViewport().extent);

        // ResizeView(view->GetRenderResource().GetViewport(), view, *pd);
    }

    // Don't include UI rendering in global render stats
    SuppressRenderStatsScope suppressRenderStatsScope;

    const ViewOutputTarget& outputTarget = m_view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    // renderCollector.BuildRenderGroups(m_view, rpl);
    ::hyperion::BuildRenderGroups(renderCollector, rpl, rpl.meshEntityOrdering, {});

    rpl.EndRead();

    renderCollector.BuildDrawCalls(0);
    renderCollector.ExecuteDrawCalls(frame, rs, outputTarget.GetFramebuffer(), 0);
}

PassData* UIRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    UIPassData* pd = new UIPassData;

    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetRenderResource().GetViewport();

    HYP_LOG(UI, Debug, "Creating UI pass data with viewport size {}", pd->viewport.extent);

    return pd;
}

#pragma endregion UIRenderer

#pragma region UIRenderSubsystem

UIRenderSubsystem::UIRenderSubsystem(const Handle<UIStage>& uiStage)
    : m_uiStage(uiStage),
      m_uiRenderer(nullptr)
{
}

UIRenderSubsystem::~UIRenderSubsystem()
{
    if (m_uiRenderer)
    {
        PUSH_RENDER_COMMAND(RemoveUIRenderer, m_uiRenderer);
        m_uiRenderer = nullptr;
    }
}

void UIRenderSubsystem::Init()
{
    HYP_SCOPE;

    m_onGbufferResolutionChangedHandle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([weakThis = WeakHandleFromThis()]()
        {
            Threads::AssertOnThread(g_renderThread);

            HYP_LOG(UI, Debug, "UIRenderSubsystem: resizing to {}", g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

            Handle<UIRenderSubsystem> subsystem = weakThis.Lock();

            if (!subsystem)
            {
                HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired on resize");

                return;
            }

            PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

            subsystem->CreateFramebuffer();
        });

    Assert(m_uiStage != nullptr);
    Assert(m_uiStage->GetCamera().IsValid());
    Assert(m_uiStage->GetCamera()->IsReady());

    m_cameraResourceHandle = TResourceHandle<RenderCamera>(m_uiStage->GetCamera()->GetRenderResource());

    const Vec2u surfaceSize = Vec2u(m_uiStage->GetSurfaceSize());
    HYP_LOG(UI, Debug, "UIRenderSubsystem: surface size is {}", surfaceSize);

    ViewOutputTargetDesc outputTargetDesc {
        .extent = surfaceSize * 2,
        .attachments = { { TF_RGBA8 }, { g_renderBackend->GetDefaultFormat(DIF_DEPTH) } }
    };

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT & ~(ViewFlags::ALL_WORLD_SCENES | ViewFlags::ENABLE_RAYTRACING),
        .viewport = Viewport { .extent = surfaceSize, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = { m_uiStage->GetScene()->HandleFromThis() },
        .camera = m_uiStage->GetCamera(),
        .drawCallCollectionImpl = GetOrCreateDrawCallCollectionImpl<UIEntityInstanceBatch>()
    };

    m_view = CreateObject<View>(viewDesc);
    InitObject(m_view);

    // temp shit
    m_view->GetRenderResource().IncRef();

    CreateFramebuffer();

    m_uiRenderer = new UIRenderer(m_view);
    m_uiRenderer->renderCollector.drawCallCollectionImpl = viewDesc.drawCallCollectionImpl;

    PUSH_RENDER_COMMAND(AddUIRenderer, m_uiRenderer);
}

void UIRenderSubsystem::OnAddedToWorld()
{
    HYP_SCOPE;

    if (m_view)
    {
        m_view->GetRenderResource().IncRef();
    }
}

void UIRenderSubsystem::OnRemovedFromWorld()
{
    if (m_view)
    {
        m_view->GetRenderResource().DecRef();
        m_view.Reset();
    }

    PUSH_RENDER_COMMAND(SetFinalPassImageView, nullptr);

    m_onGbufferResolutionChangedHandle.Reset();
}

void UIRenderSubsystem::PreUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
}

void UIRenderSubsystem::Update(float delta)
{
    m_uiStage->Update(delta);

    m_view->UpdateVisibility();

    RenderProxyList& rpl = RenderApi_GetProducerProxyList(m_view);
    rpl.BeginWrite();
    rpl.TEMP_disableBuildRenderCollection = true;
    rpl.useOrdering = true;
    rpl.viewport = m_view->GetViewport();
    rpl.priority = m_view->GetPriority();

    rpl.meshEntityOrdering.Clear();

    m_uiStage->CollectObjects([&rpl](UIObject* uiObject)
        {
            Assert(uiObject != nullptr);

            const Handle<Node>& node = uiObject->GetNode();
            Assert(node.IsValid());

            const Handle<Entity>& entity = node->GetEntity();

            MeshComponent& meshComponent = node->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(entity);

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            rpl.meshes.Track(entity.Id(), entity, entity->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ false);

            if (const Handle<Material>& material = meshComponent.material)
            {
                rpl.materials.Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                for (const auto& it : material->GetTextures())
                {
                    const Handle<Texture>& texture = it.second;

                    if (!texture.IsValid())
                    {
                        continue;
                    }

                    rpl.textures.Track(texture.Id(), texture.Get());
                }
            }

            rpl.meshEntityOrdering.EmplaceBack(entity.Id(), uiObject->GetComputedDepth());
        },
        /* onlyVisible */ true);

    ResourceTrackerDiff meshesDiff = rpl.meshes.GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> added;
        rpl.meshes.GetAdded(added, /* includeChanged */ true);

        for (Entity* entity : added)
        {
            auto&& [meshComponent, transformComponent, boundingBoxComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent>(entity);
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.meshes.SetProxy(entity->Id(), RenderProxyMesh());
            meshProxy.entity = entity->WeakHandleFromThis();
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.instanceData = meshComponent->instanceData;
            meshProxy.version = *entity->GetRenderProxyVersionPtr();
            meshProxy.bufferData.modelMatrix = transformComponent ? transformComponent->transform.GetMatrix() : Matrix4::Identity();
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            meshProxy.bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent->userData);
        }
    }

    UpdateRefs(rpl);

    rpl.EndWrite();
}

void UIRenderSubsystem::CreateFramebuffer()
{
    HYP_SCOPE;

    const ThreadId ownerThreadId = m_uiStage->GetScene()->GetEntityManager()->GetOwnerThreadId();

    auto impl = [weakThis = WeakHandleFromThis()]()
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view");

        Handle<UIRenderSubsystem> subsystem = weakThis.Lock();

        if (!subsystem)
        {
            HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired while creating view");

            return;
        }

        const Vec2u surfaceSize = Vec2u(g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

        // subsystem->m_view->SetViewport(Viewport { .extent = surfaceSize, .position = Vec2i::Zero() });

        // subsystem->m_uiStage->SetSurfaceSize(Vec2i(surfaceSize));

        HYP_LOG(UI, Debug, "Created UI Render Subsystem view with Id {} and surface size {}",
            subsystem->m_view->Id().Value(), surfaceSize);
    };

    if (Threads::IsOnThread(ownerThreadId))
    {
        HYP_NAMED_SCOPE("Create UI Render Subsystem view on owner thread");

        impl();
    }
    else
    {
        Threads::GetThread(ownerThreadId)->GetScheduler().Enqueue(std::move(impl), TaskEnqueueFlags::FIRE_AND_FORGET);
    }

    const ViewOutputTarget& outputTarget = m_view->GetOutputTarget();
    Assert(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    Assert(framebuffer.IsValid());

    const AttachmentBase* attachment = framebuffer->GetAttachment(0);
    Assert(attachment != nullptr);

    Assert(attachment->GetImageView().IsValid());
    // Assert(attachment->GetImageView()->IsCreated());

    PUSH_RENDER_COMMAND(SetFinalPassImageView, attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
