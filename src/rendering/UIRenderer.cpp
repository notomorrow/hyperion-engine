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
#include <rendering/EngineRenderStats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderTexture.hpp>

#include <rendering/font/FontAtlas.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderConfig.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <ui/UIStage.hpp>
#include <ui/UIText.hpp>

#include <scene/Mesh.hpp>
#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/World.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/UIComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <core/logging/Logger.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

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
            imageView = g_renderGlobalState->PlaceholderData->DefaultTexture2D->GetRenderResource().GetImageView();
        }

        g_engine->GetFinalPass()->SetUILayerImageView(imageView);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RebuildProxyGroups_UI)
    : RenderCommand
{
    RC<RenderProxyList> renderProxyList;
    Array<RenderProxy> addedProxies;
    Array<ObjId<Entity>> removedEntities;

    Array<Pair<ObjId<Entity>, int>> proxyDepths;

    Optional<RenderableAttributeSet> overrideAttributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<RenderProxyList>& renderProxyList,
        Array<RenderProxy*>&& addedProxyPtrs,
        Array<ObjId<Entity>>&& removedEntities,
        const Array<Pair<ObjId<Entity>, int>>& proxyDepths,
        const Optional<RenderableAttributeSet>& overrideAttributes = {})
        : renderProxyList(renderProxyList),
          removedEntities(std::move(removedEntities)),
          proxyDepths(proxyDepths),
          overrideAttributes(overrideAttributes)
    {
        addedProxies.Reserve(addedProxyPtrs.Size());

        for (RenderProxy* proxyPtr : addedProxyPtrs)
        {
            addedProxies.PushBack(*proxyPtr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override = default;

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entityAttributes) const
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
            AssertThrow(shaderDefinition.IsValid());
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

    void BuildProxyGroupsInOrder()
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: BuildProxyGroupsInOrder");

        renderProxyList->Clear();

        ResourceTracker<ObjId<Entity>, RenderProxy>& meshes = renderProxyList->meshes;

        for (const Pair<ObjId<Entity>, int>& pair : proxyDepths)
        {
            RenderProxy* proxy = meshes.GetElement(pair.first);

            if (!proxy)
            {
                continue;
            }

            const Handle<Mesh>& mesh = proxy->mesh;
            const Handle<Material>& material = proxy->material;

            if (!mesh.IsValid() || !material.IsValid())
            {
                continue;
            }

            RenderableAttributeSet attributes = GetMergedRenderableAttributes(RenderableAttributeSet {
                mesh->GetMeshAttributes(),
                material->GetRenderAttributes() });

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            attributes.SetDrawableLayer(pair.second);

            DrawCallCollectionMapping& mapping = renderProxyList->mappingsByBucket[rb][attributes];
            Handle<RenderGroup>& rg = mapping.renderGroup;

            if (!rg.IsValid())
            {
                ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();
                shaderDefinition.GetProperties().Set("INSTANCING");

                ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);
                AssertThrow(shader.IsValid());

                // Create RenderGroup
                // @NOTE: Parallel disabled for now as we don't have ParallelRenderingState for UI yet.
                rg = CreateObject<RenderGroup>(
                    shader,
                    attributes,
                    RenderGroupFlags::DEFAULT & ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::PARALLEL_RENDERING));

                rg->SetDrawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<UIEntityInstanceBatch>());

#ifdef HYP_DEBUG_MODE
                if (!rg.IsValid())
                {
                    HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                    continue;
                }
#endif

                InitObject(rg);

                if (rg->GetFlags() & RenderGroupFlags::INDIRECT_RENDERING)
                {
                    AssertDebugMsg(mapping.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

                    mapping.indirectRenderer = new IndirectRenderer();
                    mapping.indirectRenderer->Create(rg->GetDrawCallCollectionImpl());

                    mapping.drawCallCollection.impl = rg->GetDrawCallCollectionImpl();
                }
            }

            auto insertResult = mapping.renderProxies.Insert(proxy->entity.Id(), proxy);
            AssertDebug(insertResult.second);
        }

        renderProxyList->RemoveEmptyRenderGroups();
    }

    bool RemoveRenderProxy(ResourceTracker<ObjId<Entity>, RenderProxy>& meshes, ObjId<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto& mappings : renderProxyList->mappingsByBucket)
        {
            for (auto& it : mappings)
            {
                DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                auto proxyIter = mapping.renderProxies.Find(entity);

                if (proxyIter != mapping.renderProxies.End())
                {
                    mapping.renderProxies.Erase(proxyIter);

                    removed = true;

                    continue;
                }
            }
        }

        meshes.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        ResourceTracker<ObjId<Entity>, RenderProxy>& meshes = renderProxyList->meshes;

        // Claim before unclaiming items from removedEntities so modified proxies (which would be in removedEntities)
        // don't have their resources destroyed unnecessarily, causing destroy + recreate to occur much too frequently.
        for (RenderProxy& proxy : addedProxies)
        {
            proxy.IncRefs();
        }

        for (ObjId<Entity> entity : removedEntities)
        {
            const RenderProxy* proxy = meshes.GetElement(entity);
            AssertThrow(proxy != nullptr);

            proxy->DecRefs();

            meshes.MarkToRemove(entity);
        }

        for (RenderProxy& proxy : addedProxies)
        {
            const int proxyVersion = proxy.version;
            meshes.Track(proxy.entity.Id(), std::move(proxy), &proxyVersion);
        }

        meshes.Advance(AdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RenderUI)
    : RenderCommand
{
    UIRenderer* uiRenderer;
    Handle<UIStage> uiStage;
    RenderWorld* world;
    RenderView* view;

    RENDER_COMMAND(RenderUI)(UIRenderer* uiRenderer, const Handle<UIStage>& uiStage, RenderWorld* world, RenderView* view)
        : uiRenderer(uiRenderer),
          uiStage(uiStage),
          world(world),
          view(view)
    {
        AssertThrow(uiRenderer != nullptr);
        AssertThrow(uiStage.IsValid());
        AssertThrow(world != nullptr);
        AssertThrow(view != nullptr);
    }

    virtual ~RENDER_COMMAND(RenderUI)() override
    {
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Render UI");

        FrameBase* frame = g_renderBackend->GetCurrentFrame();

        RenderSetup renderSetup { world, view };

        uiRenderer->RenderFrame(frame, renderSetup);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector(),
      rpl(MakeRefCountedPtr<RenderProxyList>())
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    proxyDepths.Clear();
}

void UIRenderCollector::PushRenderProxy(ResourceTracker<ObjId<Entity>, RenderProxy>& meshes, const RenderProxy& renderProxy, int computedDepth)
{
    AssertThrow(renderProxy.entity.IsValid());
    AssertThrow(renderProxy.mesh.IsValid());
    AssertThrow(renderProxy.material.IsValid());

    meshes.Track(renderProxy.entity, renderProxy, &renderProxy.version);

    proxyDepths.EmplaceBack(renderProxy.entity.Id(), computedDepth);
}

typename ResourceTracker<ObjId<Entity>, RenderProxy>::Diff UIRenderCollector::PushUpdatesToRenderThread(ResourceTracker<ObjId<Entity>, RenderProxy>& meshes, const Optional<RenderableAttributeSet>& overrideAttributes)
{
    HYP_SCOPE;

    // UISubsystem can have Update() called on a task thread.
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    auto diff = meshes.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<ObjId<Entity>> removedEntities;
        meshes.GetRemoved(removedEntities, true /* includeChanged */);

        Array<RenderProxy*> addedProxiesPtrs;
        meshes.GetAdded(addedProxiesPtrs, true /* includeChanged */);

        if (addedProxiesPtrs.Any() || removedEntities.Any())
        {
            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                rpl,
                std::move(addedProxiesPtrs),
                std::move(removedEntities),
                proxyDepths,
                overrideAttributes);
        }
    }

    meshes.Advance(AdvanceAction::CLEAR);

    return diff;
}

void UIRenderCollector::CollectDrawCalls(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    RenderCollector::CollectDrawCalls(*rpl, 0);
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_renderThread);

    AssertThrow(rpl != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frameIndex);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& mappings : rpl->mappingsByBucket)
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
        AssertThrow(mapping.IsValid());

        const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
        AssertThrow(renderGroup.IsValid());

        const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

        // Don't count draw calls for UI
        SuppressEngineRenderStatsScope suppressRenderStatsScope;

        renderGroup->PerformRendering(frame, renderSetup, drawCallCollection, nullptr, nullptr);
    }

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frameIndex);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderer

void UIRenderer::Initialize()
{
}

void UIRenderer::Shutdown()
{
}

void UIRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    View* view = renderSetup.view->GetView();
    AssertThrow(view != nullptr);

    UIPassData* pd = static_cast<UIPassData*>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    if (pd->viewport != view->GetRenderResource().GetViewport())
    {
        /// @TODO: Implement me!

        HYP_LOG(UI, Warning, "UIRenderer: Viewport size changed from {} to {}, resizing view pass data",
            pd->viewport.extent, view->GetRenderResource().GetViewport().extent);

        // ResizeView(view->GetRenderResource().GetViewport(), view, *pd);
    }

    RenderSetup rs = renderSetup;
    rs.passData = pd;

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertThrow(outputTarget.IsValid());

    m_renderCollector.CollectDrawCalls(frame, rs);
    m_renderCollector.ExecuteDrawCalls(frame, rs, outputTarget.GetFramebuffer());
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
    // push render command to delete UIRenderer on the render thread
    if (m_uiRenderer)
    {
        struct RENDER_COMMAND(DeleteUIRenderer)
            : RenderCommand
        {
            UIRenderer* uiRenderer;

            RENDER_COMMAND(DeleteUIRenderer)(UIRenderer* uiRenderer)
                : uiRenderer(uiRenderer)
            {
            }

            virtual ~RENDER_COMMAND(DeleteUIRenderer)() override = default;

            virtual RendererResult operator()() override
            {
                delete uiRenderer;

                HYPERION_RETURN_OK;
            }
        };

        PUSH_RENDER_COMMAND(DeleteUIRenderer, m_uiRenderer);
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

    AssertThrow(m_uiStage != nullptr);
    AssertThrow(m_uiStage->GetCamera().IsValid());
    AssertThrow(m_uiStage->GetCamera()->IsReady());

    m_uiRenderer = new UIRenderer();

    m_cameraResourceHandle = TResourceHandle<RenderCamera>(m_uiStage->GetCamera()->GetRenderResource());

    const Vec2u surfaceSize = Vec2u(m_uiStage->GetSurfaceSize());
    HYP_LOG(UI, Debug, "UIRenderSubsystem: surface size is {}", surfaceSize);

    ViewOutputTargetDesc outputTargetDesc {
        .extent = surfaceSize * 2,
        .attachments = { { TF_RGBA8 }, { g_renderBackend->GetDefaultFormat(DIF_DEPTH) } }
    };

    ViewDesc viewDesc {
        .flags = ViewFlags::DEFAULT & ~ViewFlags::ALL_WORLD_SCENES,
        .viewport = Viewport { .extent = surfaceSize, .position = Vec2i::Zero() },
        .outputTargetDesc = outputTargetDesc,
        .scenes = { m_uiStage->GetScene()->HandleFromThis() },
        .camera = m_uiStage->GetCamera()
    };

    m_view = CreateObject<View>(viewDesc);
    InitObject(m_view);

    // temp shit
    m_view->GetRenderResource().IncRef();

    CreateFramebuffer();
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

    m_uiStage->Update(delta);
}

void UIRenderSubsystem::Update(float delta)
{
    UIRenderCollector& renderCollector = m_uiRenderer->GetRenderCollector();
    renderCollector.ResetOrdering();

    m_uiStage->CollectObjects([&renderCollector, &meshes = m_renderProxyTracker](UIObject* uiObject)
        {
            AssertThrow(uiObject != nullptr);

            const Handle<Node>& node = uiObject->GetNode();
            AssertThrow(node.IsValid());

            const Handle<Entity>& entity = node->GetEntity();

            MeshComponent& meshComponent = node->GetScene()->GetEntityManager()->GetComponent<MeshComponent>(entity);

            if (!meshComponent.proxy)
            {
                return;
            }

            // @TODO Include a way to determine the parent tree of the UI Object because some objects will
            // have the same depth but should be rendered in a different order.
            renderCollector.PushRenderProxy(meshes, *meshComponent.proxy, uiObject->GetComputedDepth());
        },
        /* onlyVisible */ true);

    renderCollector.PushUpdatesToRenderThread(m_renderProxyTracker);

    PUSH_RENDER_COMMAND(RenderUI, m_uiRenderer, m_uiStage, &GetWorld()->GetRenderResource(), &m_view->GetRenderResource());
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
    AssertThrow(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertThrow(framebuffer.IsValid());

    const AttachmentBase* attachment = framebuffer->GetAttachment(0);
    AssertThrow(attachment != nullptr);

    AssertThrow(attachment->GetImageView().IsValid());
    // AssertThrow(attachment->GetImageView()->IsCreated());

    PUSH_RENDER_COMMAND(SetFinalPassImageView, attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
