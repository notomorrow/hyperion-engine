/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderView.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <Engine.hpp>

namespace hyperion {

static constexpr bool do_parallel_collection = true;

#pragma region Render commands

struct RENDER_COMMAND(RebuildProxyGroups) : renderer::RenderCommand
{
    TResourceHandle<ViewRenderResource> view_render_resource_handle;

    RC<EntityDrawCollection>            collection;
    Array<RenderProxy>                  added_proxies;
    Array<ID<Entity>>                   removed_proxies;

    Handle<Camera>                      camera;
    Optional<RenderableAttributeSet>    override_attributes;

    RENDER_COMMAND(RebuildProxyGroups)(
        const TResourceHandle<ViewRenderResource> &view_render_resource_handle,
        const RC<EntityDrawCollection> &collection,
        Array<RenderProxy *> &&added_proxy_ptrs,
        Array<ID<Entity>> &&removed_proxies,
        const Handle<Camera> &camera = Handle<Camera>::empty,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    ) : view_render_resource_handle(view_render_resource_handle),
        collection(collection),
        removed_proxies(std::move(removed_proxies)),
        camera(camera),
        override_attributes(override_attributes)
    {
        AssertThrow(view_render_resource_handle);

        added_proxies.Reserve(added_proxy_ptrs.Size());

        for (RenderProxy *proxy_ptr : added_proxy_ptrs) {
            added_proxies.PushBack(*proxy_ptr);
        }
    }

    virtual ~RENDER_COMMAND(RebuildProxyGroups)() override = default;

    RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy &proxy) const
    {
        HYP_SCOPE;
        
        const Handle<Mesh> &mesh = proxy.mesh;
        AssertThrow(mesh.IsValid());

        const Handle<Material> &material = proxy.material;
        AssertThrow(material.IsValid());

        RenderableAttributeSet attributes {
            mesh->GetMeshAttributes(),
            material->GetRenderAttributes()
        };

        if (override_attributes.HasValue()) {
            if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
                attributes.SetShaderDefinition(override_shader_definition);
            }
            
            const ShaderDefinition &shader_definition = override_attributes->GetShaderDefinition().IsValid()
                ? override_attributes->GetShaderDefinition()
                : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
            AssertThrow(shader_definition.IsValid());
#endif

            // Check for varying vertex attributes on the override shader compared to the entity's vertex
            // attributes. If there is not a match, we should switch to a version of the override shader that
            // has matching vertex attribs.
            const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

            MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
            new_material_attributes.shader_definition = shader_definition;

            if (mesh_vertex_attributes != new_material_attributes.shader_definition.GetProperties().GetRequiredVertexAttributes()) {
                new_material_attributes.shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
            }

            // do not override bucket!
            new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

            attributes.SetMaterialAttributes(new_material_attributes);
        }

        return attributes;
    }

    void AddRenderProxy(RenderProxyTracker &render_proxy_tracker, RenderProxy &&proxy, const RenderableAttributeSet &attributes, Bucket bucket)
    {
        HYP_SCOPE;

        const ID<Entity> entity = proxy.entity.GetID();

        // Add proxy to group
        Handle<RenderGroup> &render_group = collection->GetProxyGroups()[uint32(bucket)][attributes];

        if (!render_group.IsValid()) {
            HYP_LOG(RenderCollection, Debug, "Creating RenderGroup for attributes:\n"
                "\tVertex Attributes: {}\n"
                "\tBucket: {}\n"
                "\tShader Definition: {}  {}\n"
                "\tCull Mode: {}\n"
                "\tFlags: {}\n"
                "\tDrawable layer: {}",
                attributes.GetMeshAttributes().vertex_attributes.flag_mask,
                uint32(attributes.GetMaterialAttributes().bucket),
                attributes.GetShaderDefinition().GetName(),
                attributes.GetShaderDefinition().GetProperties().GetHashCode().Value(),
                uint32(attributes.GetMaterialAttributes().cull_faces),
                uint32(attributes.GetMaterialAttributes().flags),
                attributes.GetDrawableLayer());

            EnumFlags<RenderGroupFlags> render_group_flags = RenderGroupFlags::DEFAULT;

            // Disable occlusion culling for translucent objects
            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            if (bucket == BUCKET_TRANSLUCENT || bucket == BUCKET_DEBUG) {
                render_group_flags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
            }

            // Create RenderGroup
            render_group = CreateObject<RenderGroup>(
                g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
                attributes,
                render_group_flags
            );

            FramebufferRef framebuffer;

            if (camera.IsValid()) {
                framebuffer = camera->GetRenderResource().GetFramebuffer();
            }

            if (framebuffer != nullptr) {
                render_group->AddFramebuffer(framebuffer);
            } else {
                AssertThrow(view_render_resource_handle->GetView()->GetFlags() & ViewFlags::GBUFFER);

                GBuffer *gbuffer = view_render_resource_handle->GetGBuffer();
                AssertThrow(gbuffer != nullptr);

                const FramebufferRef &bucket_framebuffer = gbuffer->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
                AssertThrow(bucket_framebuffer != nullptr);

                render_group->AddFramebuffer(bucket_framebuffer);
            }

            InitObject(render_group);
        }

        auto iter = render_proxy_tracker.Track(entity, std::move(proxy));

        render_group->AddRenderProxy(iter->second);
    }

    bool RemoveRenderProxy(RenderProxyTracker &render_proxy_tracker, ID<Entity> entity, const RenderableAttributeSet &attributes, Bucket bucket)
    {
        HYP_SCOPE;

        auto &render_groups_by_attributes = collection->GetProxyGroups()[uint32(bucket)];

        auto it = render_groups_by_attributes.Find(attributes);
        AssertThrow(it != render_groups_by_attributes.End());

        const Handle<RenderGroup> &render_group = it->second;
        AssertThrow(render_group.IsValid());

        const bool removed = render_group->RemoveRenderProxy(entity);

        render_proxy_tracker.MarkToRemove(entity);

        return removed;
    }

    virtual RendererResult operator()() override
    {
        HYP_SCOPE;

        RenderProxyTracker &render_proxy_tracker = collection->GetRenderProxyTracker();

        // Reserve to prevent iterator invalidation (pointers to proxies are stored in the render groups)
        render_proxy_tracker.Reserve(added_proxies.Size());

#if 0
        HYP_LOG(RenderCollection, Debug,
            "Added: {} | Removed: {}",
            added_proxies.Size(),
            removed_proxies.Size()
        );

        for (const auto &added : added_proxies) {
            HYP_LOG(
                RenderCollection,
                Debug,
                "Added proxy for entity {} (mesh={}, count: {}, material={}, count: {})",
                added.entity.GetID().Value(),
                added.mesh ? *added.mesh->GetName() : "null", added.mesh ? added.mesh->GetRenderResource().NumClaims() : 0,
                added.material ? *added.material->GetName() : "null", added.material ? added.material->GetRenderResource().NumClaims() : 0
            );
        }

        for (const auto &removed : removed_proxies) {
            HYP_LOG(
                RenderCollection,
                Debug,
                "Removed proxy for entity {}",
                removed.Value()
            );
        }
#endif
        for (RenderProxy &proxy : added_proxies) {
            proxy.ClaimRenderResource();
        }

        for (ID<Entity> entity_id : removed_proxies) {
            const RenderProxy *proxy = render_proxy_tracker.GetElement(entity_id);
            AssertThrowMsg(proxy != nullptr, "Proxy is missing for Entity #%u", entity_id.Value());

            proxy->UnclaimRenderResource();

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy);
            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            AssertThrow(RemoveRenderProxy(render_proxy_tracker, entity_id, attributes, bucket));
        }

        for (RenderProxy &proxy : added_proxies) {
            const Handle<Mesh> &mesh = proxy.mesh;
            AssertThrow(mesh.IsValid());
            AssertThrow(mesh->IsReady());

            const Handle<Material> &material = proxy.material;
            AssertThrow(material.IsValid());

            const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy);
            const Bucket bucket = attributes.GetMaterialAttributes().bucket;

            AddRenderProxy(render_proxy_tracker, std::move(proxy), attributes, bucket);
        }

        render_proxy_tracker.Advance(RenderProxyListAdvanceAction::PERSIST);
        
        // Clear out groups that are no longer used
        collection->RemoveEmptyProxyGroups();

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region EntityDrawCollection

void EntityDrawCollection::ClearProxyGroups()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto &render_groups_by_attributes : GetProxyGroups()) {
        for (auto &it : render_groups_by_attributes) {
            it.second->ClearProxies();
        }
    }
}

void EntityDrawCollection::RemoveEmptyProxyGroups()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    for (auto &render_groups_by_attributes : GetProxyGroups()) {
        for (auto it = render_groups_by_attributes.Begin(); it != render_groups_by_attributes.End();) {
            if (it->second->GetRenderProxies().Any()) {
                ++it;

                continue;
            }

            it = render_groups_by_attributes.Erase(it);
        }
    }
}

uint32 EntityDrawCollection::NumRenderGroups() const
{
    Threads::AssertOnThread(g_render_thread);

    uint32 count = 0;

    for (const auto &render_groups_by_attributes : m_proxy_groups) {
        for (const auto &it : render_groups_by_attributes) {
            if (it.second.IsValid()) {
                ++count;
            }
        }
    }

    return count;
}

#pragma endregion EntityDrawCollection

#pragma region RenderCollector

RenderCollector::RenderCollector()
    : m_draw_collection(MakeRefCountedPtr<EntityDrawCollection>())
{
}

RenderCollector::~RenderCollector() = default;

void RenderCollector::CollectDrawCalls(
    FrameBase *frame,
    ViewRenderResource *view,
    const Bitset &bucket_bits,
    const CullData *cull_data
)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (bucket_bits.Count() == 0) {
        return;
    }

    HYP_MT_CHECK_READ(m_data_race_detector);

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();

    using IteratorType = FlatMap<RenderableAttributeSet, Handle<RenderGroup>>::Iterator;

    Array<IteratorType> iterators;

    for (Bitset::BitIndex bit_index : bucket_bits) {
        auto &render_groups_by_attributes = m_draw_collection->GetProxyGroups()[bit_index];

        if (render_groups_by_attributes.Empty()) {
            continue;
        }

        for (auto &it : render_groups_by_attributes) {
            iterators.PushBack(&it);
        }
    }

    if (iterators.Empty()) {
        return;
    }

    if (do_parallel_collection && iterators.Size() > 1) {
        TaskSystem::GetInstance().ParallelForEach(
            HYP_STATIC_MESSAGE("RenderCollector::CollectDrawCalls"),
            TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER),
            iterators,
            [this](IteratorType it, uint32, uint32)
            {
                Handle<RenderGroup> &render_group = it->second;
                AssertThrow(render_group.IsValid());

                render_group->CollectDrawCalls();
            }
        );
    } else {
        for (IteratorType it : iterators) {
            Handle<RenderGroup> &render_group = it->second;
            AssertThrow(render_group.IsValid());

            render_group->CollectDrawCalls();
        }
    }

    if (is_indirect_rendering_enabled && cull_data != nullptr) {
        for (SizeType index = 0; index < iterators.Size(); index++) {
            if (!(iterators[index]->second->GetFlags() & RenderGroupFlags::OCCLUSION_CULLING)) {
                continue;
            }

            (*iterators[index]).second->PerformOcclusionCulling(frame, view, cull_data);
        }
    }
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase *frame,
    ViewRenderResource *view,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    AssertThrow(view != nullptr);

    const FramebufferRef &framebuffer = view->GetCamera()->GetFramebuffer();
    AssertThrowMsg(framebuffer, "Camera has no Framebuffer attached");

    ExecuteDrawCalls(frame, view, framebuffer, bucket_bits, cull_data, push_constant);
}

void RenderCollector::ExecuteDrawCalls(
    FrameBase *frame,
    ViewRenderResource *view,
    const FramebufferRef &framebuffer,
    const Bitset &bucket_bits,
    const CullData *cull_data,
    PushConstantData push_constant
) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    HYP_MT_CHECK_READ(m_data_race_detector);

    static const bool is_indirect_rendering_enabled = g_rendering_api->GetRenderConfig().IsIndirectRenderingEnabled();
    
    AssertThrow(m_draw_collection != nullptr);

    const uint32 frame_index = frame->GetFrameIndex();

    if (bucket_bits.Count() == 0) {
        return;
    }

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (bucket_bits.Count() == 1) {
        const Bucket bucket = Bucket(MathUtil::FastLog2_Pow2(bucket_bits.ToUInt64()));

        const auto &render_groups_by_attributes = m_draw_collection->GetProxyGroups()[uint32(bucket)];

        if (render_groups_by_attributes.Empty()) {
            return;
        }

        if (framebuffer) {
            frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
        }

        for (const auto &it : render_groups_by_attributes) {
            const RenderableAttributeSet &attributes = it.first;
            const Handle<RenderGroup> &render_group = it.second;

            if (push_constant) {
                render_group->GetPipeline()->SetPushConstants(push_constant.Data(), push_constant.Size());
            }

            if (is_indirect_rendering_enabled && cull_data != nullptr && (render_group->GetFlags() & RenderGroupFlags::INDIRECT_RENDERING)) {
                render_group->PerformRenderingIndirect(frame, view);
            } else {
                render_group->PerformRendering(frame, view);
            }
        }

        if (framebuffer) {
            frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
        }
    } else {
        bool all_empty = false;

        for (const auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
            if (render_groups_by_attributes.Any()) {
                all_empty = false;

                break;
            }
        }

        if (all_empty) {
            return;
        }

        if (framebuffer) {
            frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frame_index);
        }

        for (const auto &render_groups_by_attributes : m_draw_collection->GetProxyGroups()) {
            for (const auto &it : render_groups_by_attributes) {
                const RenderableAttributeSet &attributes = it.first;
                const Handle<RenderGroup> &render_group = it.second;

                const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                if (!bucket_bits.Test(uint32(bucket))) {
                    continue;
                }

                if (push_constant) {
                    render_group->GetPipeline()->SetPushConstants(push_constant.Data(), push_constant.Size());
                }

                if (is_indirect_rendering_enabled && cull_data != nullptr && (render_group->GetFlags() & RenderGroupFlags::INDIRECT_RENDERING)) {
                    render_group->PerformRenderingIndirect(frame, view);
                } else {
                    render_group->PerformRendering(frame, view);
                }
            }
        }

        if (framebuffer) {
            frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frame_index);
        }
    }
}

#pragma endregion RenderCollector

} // namespace hyperion