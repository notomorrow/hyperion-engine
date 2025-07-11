/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/UIRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/EngineRenderStats.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Renderer.hpp>

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
    Vec4f texcoords[max_entities_per_instance_batch];
    Vec4f offsets[max_entities_per_instance_batch];
    Vec4f sizes[max_entities_per_instance_batch];
};

static_assert(sizeof(UIEntityInstanceBatch) == 6976);

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups_UI)
    : renderer::RenderCommand
{
    RC<EntityDrawCollection> collection;
    Array<RenderProxy> added_proxies;
    Array<ID<Entity>> removed_entities;

    Array<Pair<ID<Entity>, int>> proxy_depths;

    FramebufferRef framebuffer;

    Optional<RenderableAttributeSet> override_attributes;

    RENDER_COMMAND(RebuildProxyGroups_UI)(
        const RC<EntityDrawCollection>& collection,
        Array<RenderProxy*>&& added_proxy_ptrs,
        Array<ID<Entity>>&& removed_entities,
        const Array<Pair<ID<Entity>, int>>& proxy_depths,
        const FramebufferRef& framebuffer,
        const Optional<RenderableAttributeSet>& override_attributes = {})
        : collection(collection),
          removed_entities(std::move(removed_entities)),
          proxy_depths(proxy_depths),
          framebuffer(framebuffer),
          override_attributes(override_attributes)
    {
        added_proxies.Reserve(added_proxy_ptrs.Size());

        for (RenderProxy* proxy_ptr : added_proxy_ptrs)
        {
            added_proxies.PushBack(*proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups_UI)() override = default;

    RenderableAttributeSet GetMergedRenderableAttributes(const RenderableAttributeSet& entity_attributes) const
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: GetMergedRenderableAttributes");

        RenderableAttributeSet attributes = entity_attributes;

        if (override_attributes.HasValue())
        {
            if (const ShaderDefinition& override_shader_definition = override_attributes->GetShaderDefinition())
            {
                attributes.SetShaderDefinition(override_shader_definition);
            }

            ShaderDefinition shader_definition = override_attributes->GetShaderDefinition().IsValid()
                ? override_attributes->GetShaderDefinition()
                : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
            AssertThrow(shader_definition.IsValid());
#endif

            // Check for varying vertex attributes on the override shader compared to the entity's vertex
            // attributes. If there is not a match, we should switch to a version of the override shader that
            // has matching vertex attribs.
            const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

            if (mesh_vertex_attributes != shader_definition.GetProperties().GetRequiredVertexAttributes())
            {
                shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
            }

            MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
            new_material_attributes.shader_definition = shader_definition;
            // do not override bucket!
            new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetMaterialAttributes(new_material_attributes);
        }

        return attributes;
    }

    void BuildProxyGroupsInOrder()
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups: BuildProxyGroupsInOrder");

        collection->ClearProxyGroups();

        RenderProxyTracker& render_proxy_tracker = collection->GetRenderProxyTracker();

        for (const Pair<ID<Entity>, int>& pair : proxy_depths)
        {
            RenderProxy* proxy = render_proxy_tracker.GetElement(pair.first);

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

            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetDrawableLayer(pair.second);

            Handle<RenderGroup>& render_group = collection->GetProxyGroups()[uint32(bucket)][attributes];

            if (!render_group.IsValid())
            {
                ShaderDefinition shader_definition = attributes.GetShaderDefinition();
                shader_definition.GetProperties().Set("INSTANCING");

                ShaderRef shader = g_shader_manager->GetOrCreate(shader_definition);
                AssertThrow(shader.IsValid());

                // Create RenderGroup
                // @NOTE: Parallel disabled for now as we don't have ParallelRenderingState for UI yet.
                render_group = CreateObject<RenderGroup>(
                    shader,
                    attributes,
                    RenderGroupFlags::DEFAULT & ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::PARALLEL_RENDERING));

                render_group->SetDrawCallCollectionImpl(GetOrCreateDrawCallCollectionImpl<UIEntityInstanceBatch>());

                render_group->AddFramebuffer(framebuffer);

                HYP_LOG(UI, Debug, "Create render group {} (#{})", attributes.GetHashCode().Value(), render_group.GetID().Value());

#ifdef HYP_DEBUG_MODE
                if (!render_group.IsValid())
                {
                    HYP_LOG(UI, Error, "Render group not valid for attribute set {}!", attributes.GetHashCode().Value());

                    continue;
                }
#endif

                InitObject(render_group);
            }

            render_group->AddRenderProxy(proxy);
        }

        collection->RemoveEmptyProxyGroups();
    }

    bool RemoveRenderProxy(RenderProxyTracker& render_proxy_tracker, ID<Entity> entity)
    {
        HYP_SCOPE;

        bool removed = false;

        for (auto& render_groups_by_attributes : collection->GetProxyGroups())
        {
            for (auto& it : render_groups_by_attributes)
            {
                removed |= it.second->RemoveRenderProxy(entity);
            }
        }

        render_proxy_tracker.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_NAMED_SCOPE("Rebuild UI Proxy Groups");

        RenderProxyTracker& render_proxy_tracker = collection->GetRenderProxyTracker();

        // Reserve to prevent iterator invalidation
        render_proxy_tracker.Reserve(added_proxies.Size());

        // Claim before unclaiming items from removed_entities so modified proxies (which would be in removed_entities)
        // don't have their resources destroyed unnecessarily, causing destroy + recreate to occur much too frequently.
        for (RenderProxy& proxy : added_proxies)
        {
            proxy.IncRefs();
        }

        for (ID<Entity> entity : removed_entities)
        {
            const RenderProxy* proxy = render_proxy_tracker.GetElement(entity);
            AssertThrow(proxy != nullptr);

            proxy->DecRefs();

            render_proxy_tracker.MarkToRemove(entity);
        }

        for (RenderProxy& proxy : added_proxies)
        {
            render_proxy_tracker.Track(proxy.entity.GetID(), std::move(proxy));
        }

        render_proxy_tracker.Advance(RenderProxyListAdvanceAction::PERSIST);

        BuildProxyGroupsInOrder();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region UIRenderCollector

UIRenderCollector::UIRenderCollector()
    : RenderCollector()
{
}

UIRenderCollector::~UIRenderCollector() = default;

void UIRenderCollector::ResetOrdering()
{
    m_proxy_depths.Clear();
}

void UIRenderCollector::PushRenderProxy(RenderProxyTracker& render_proxy_tracker, const RenderProxy& render_proxy, int computed_depth)
{
    AssertThrow(render_proxy.entity.IsValid());
    AssertThrow(render_proxy.mesh.IsValid());
    AssertThrow(render_proxy.material.IsValid());

    render_proxy_tracker.Track(render_proxy.entity, render_proxy);

    m_proxy_depths.EmplaceBack(render_proxy.entity.GetID(), computed_depth);
}

typename RenderProxyTracker::Diff UIRenderCollector::PushUpdatesToRenderThread(RenderProxyTracker& render_proxy_tracker, const FramebufferRef& framebuffer, const Optional<RenderableAttributeSet>& override_attributes)
{
    HYP_SCOPE;

    // UISubsystem can have Update() called on a task thread.
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    typename RenderProxyTracker::Diff diff = render_proxy_tracker.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<ID<Entity>> removed_entities;
        render_proxy_tracker.GetRemoved(removed_entities, true /* include_changed */);

        Array<RenderProxy*> added_proxies_ptrs;
        render_proxy_tracker.GetAdded(added_proxies_ptrs, true /* include_changed */);

        if (added_proxies_ptrs.Any() || removed_entities.Any())
        {
            PUSH_RENDER_COMMAND(
                RebuildProxyGroups_UI,
                m_draw_collection,
                std::move(added_proxies_ptrs),
                std::move(removed_entities),
                m_proxy_depths,
                framebuffer,
                override_attributes);
        }
    }

    render_proxy_tracker.Advance(RenderProxyListAdvanceAction::CLEAR);

    return diff;
}

void UIRenderCollector::CollectDrawCalls(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::Iterator;

    Array<IteratorType> iterators;

    for (auto& render_groups_by_attributes : m_draw_collection->GetProxyGroups())
    {
        for (auto& it : render_groups_by_attributes)
        {
            const RenderableAttributeSet& attributes = it.first;

            iterators.PushBack(&it);
        }
    }

    TaskSystem::GetInstance().ParallelForEach(
        TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER),
        iterators,
        [this](IteratorType it, uint32, uint32)
        {
            Handle<RenderGroup>& render_group = it->second;
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls();
        });
}

void UIRenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_draw_collection != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
    }

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::ConstIterator;
    Array<IteratorType> iterators;

    for (const auto& render_groups_by_attributes : m_draw_collection->GetProxyGroups())
    {
        for (const auto& it : render_groups_by_attributes)
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
        const Handle<RenderGroup>& render_group = it.second;

        AssertThrow(render_group.IsValid());

        // Don't count draw calls for UI
        SuppressEngineRenderStatsScope suppress_render_stats_scope;

        render_group->PerformRendering(frame, render_setup, nullptr);
    }

    if (framebuffer.IsValid())
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
    }
}

#pragma endregion UIRenderCollector

#pragma region UIRenderSubsystem

UIRenderSubsystem::UIRenderSubsystem(Name name, const RC<UIStage>& ui_stage)
    : RenderSubsystem(name),
      m_ui_stage(ui_stage)
{
}

UIRenderSubsystem::~UIRenderSubsystem()
{
}

void UIRenderSubsystem::Init()
{
    HYP_SCOPE;

    struct RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)
        : renderer::RenderCommand
    {
        RC<UIRenderSubsystem> ui_render_subsystem;

        RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)(const RC<UIRenderSubsystem>& ui_render_subsystem)
            : ui_render_subsystem(ui_render_subsystem)
        {
            AssertThrow(ui_render_subsystem != nullptr);
        }

        virtual ~RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer)() override = default;

        virtual RendererResult operator()() override
        {
            ui_render_subsystem->CreateFramebuffer();

            HYPERION_RETURN_OK;
        }
    };

    m_on_gbuffer_resolution_changed_handle = g_engine->GetDelegates().OnAfterSwapchainRecreated.Bind([weak_this = WeakRefCountedPtrFromThis()]()
        {
            Threads::AssertOnThread(g_render_thread);

            HYP_LOG(UI, Debug, "UIRenderSubsystem: resizing to {}", g_engine->GetAppContext()->GetMainWindow()->GetDimensions());

            RC<UIRenderSubsystem> subsystem = weak_this.Lock().CastUnsafe<UIRenderSubsystem>();

            if (!subsystem)
            {
                HYP_LOG(UI, Warning, "UIRenderSubsystem: subsystem is expired on resize");

                return;
            }

            g_engine->GetFinalPass()->SetUILayerImageView(g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

            SafeRelease(std::move(subsystem->m_framebuffer));

            PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, subsystem);
        });

    AssertThrow(m_ui_stage != nullptr);
    AssertThrow(m_ui_stage->GetCamera().IsValid());
    AssertThrow(m_ui_stage->GetCamera()->IsReady());

    m_camera_resource_handle = TResourceHandle<RenderCamera>(m_ui_stage->GetCamera()->GetRenderResource());

    m_view = CreateObject<View>(ViewDesc {
        .flags = ViewFlags::DEFAULT & ~ViewFlags::ALL_WORLD_SCENES,
        .viewport = Viewport { .extent = m_ui_stage->GetSurfaceSize(), .position = Vec2i::Zero() },
        .scenes = { m_ui_stage->GetScene()->HandleFromThis() },
        .camera = m_ui_stage->GetCamera() });
    InitObject(m_view);

    m_render_view = TResourceHandle<RenderView>(m_view->GetRenderResource());

    PUSH_RENDER_COMMAND(CreateUIRenderSubsystemFramebuffer, RefCountedPtrFromThis().CastUnsafe<UIRenderSubsystem>());
}

// called from game thread
void UIRenderSubsystem::InitGame()
{
}

void UIRenderSubsystem::OnRemoved()
{
    m_render_view.Reset();
    m_view.Reset();

    g_engine->GetFinalPass()->SetUILayerImageView(g_engine->GetPlaceholderData()->GetImageView2D1x1R8());

    SafeRelease(std::move(m_framebuffer));

    m_on_gbuffer_resolution_changed_handle.Reset();
}

void UIRenderSubsystem::OnUpdate(GameCounter::TickUnit delta)
{
    // do nothing
}

void UIRenderSubsystem::OnRender(FrameBase* frame, const RenderSetup&)
{
    HYP_SCOPE;

    const RenderSetup render_setup { &g_engine->GetWorld()->GetRenderResource(), m_render_view.Get() };

    m_render_collector.CollectDrawCalls(frame, render_setup);
    m_render_collector.ExecuteDrawCalls(frame, render_setup, m_framebuffer);
}

void UIRenderSubsystem::CreateFramebuffer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const Vec2i surface_size = g_engine->GetAppContext()->GetMainWindow()->GetDimensions();

    m_render_view->SetViewport(Viewport {
        .extent = surface_size,
        .position = Vec2i::Zero() });

    m_framebuffer = g_rendering_api->MakeFramebuffer(Vec2u(surface_size) * 2);

    AttachmentRef color_attachment = m_framebuffer->AddAttachment(
        0,
        InternalFormat::RGBA16F,
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    m_framebuffer->AddAttachment(
        1,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_2D,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE);

    DeferCreate(m_framebuffer);

    m_camera_resource_handle->SetFramebuffer(m_framebuffer);

    m_ui_stage->GetScene()->GetEntityManager()->PushCommand([ui_stage = m_ui_stage, surface_size](EntityManager& mgr, GameCounter::TickUnit delta)
        {
            ui_stage->SetSurfaceSize(surface_size);
        });

    g_engine->GetFinalPass()->SetUILayerImageView(color_attachment->GetImageView());
}

#pragma endregion UIRenderSubsystem

} // namespace hyperion
