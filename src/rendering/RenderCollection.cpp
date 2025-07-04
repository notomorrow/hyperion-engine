/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderCollection.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderMaterial.hpp>
#include <rendering/RenderView.hpp>

#include <rendering/backend/RenderBackend.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>    // For LightType
#include <scene/EnvProbe.hpp> // For EnvProbeType

#include <scene/camera/Camera.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/Util.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

static constexpr bool doParallelCollection = false; // true;

#pragma region RenderProxyList

static RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxyMesh& proxy, const RenderableAttributeSet* overrideAttributes = nullptr)
{
    HYP_SCOPE;

    const Handle<Mesh>& mesh = proxy.mesh;
    Assert(mesh.IsValid());

    const Handle<Material>& material = proxy.material;
    Assert(material.IsValid());

    RenderableAttributeSet attributes {
        mesh->GetMeshAttributes(),
        material->GetRenderAttributes()
    };

    if (overrideAttributes)
    {
        if (const ShaderDefinition& overrideShaderDefinition = overrideAttributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(overrideShaderDefinition);
        }

        const ShaderDefinition& shaderDefinition = overrideAttributes->GetShaderDefinition().IsValid()
            ? overrideAttributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        Assert(shaderDefinition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet meshVertexAttributes = attributes.GetMeshAttributes().vertexAttributes;

        MaterialAttributes newMaterialAttributes = overrideAttributes->GetMaterialAttributes();
        newMaterialAttributes.shaderDefinition = shaderDefinition;

        if (meshVertexAttributes != newMaterialAttributes.shaderDefinition.GetProperties().GetRequiredVertexAttributes())
        {
            newMaterialAttributes.shaderDefinition.properties.SetRequiredVertexAttributes(meshVertexAttributes);
        }

        // do not override bucket!
        newMaterialAttributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(newMaterialAttributes);
    }

    return attributes;
}

static void UpdateRenderableAttributesDynamic(const RenderProxyMesh* proxy, RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    union
    {
        struct
        {
            bool hasInstancing : 1;
            bool hasForwardLighting : 1;
            bool hasAlphaDiscard : 1;
        };

        uint64 overridden;
    };

    overridden = 0;

    hasInstancing = proxy->instanceData.enableAutoInstancing || proxy->instanceData.numInstances > 1;
    hasForwardLighting = attributes.GetMaterialAttributes().bucket == RB_TRANSLUCENT;
    hasAlphaDiscard = bool(attributes.GetMaterialAttributes().flags & MAF_ALPHA_DISCARD);

    if (!overridden)
    {
        return;
    }

    bool shaderDefinitionChanged = false;
    ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();

    if (hasInstancing)
    {
        shaderDefinition.GetProperties().Set("INSTANCING");
        shaderDefinitionChanged = true;
    }

    if (hasForwardLighting)
    {
        shaderDefinition.GetProperties().Set("FORWARD_LIGHTING");
        shaderDefinitionChanged = true;
    }

    if (hasAlphaDiscard)
    {
        shaderDefinition.GetProperties().Set("ALPHA_DISCARD");
        shaderDefinitionChanged = true;
    }

    if (shaderDefinitionChanged)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderDefinition(shaderDefinition);
    }
}

static void AddRenderProxy(RenderProxyList* renderProxyList, ResourceTracker<ObjId<Entity>, RenderProxyMesh>& meshes, RenderProxyMesh* proxy, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    // Add proxy to group
    DrawCallCollectionMapping& mapping = renderProxyList->mappingsByBucket[rb][attributes];
    Handle<RenderGroup>& rg = mapping.renderGroup;

    if (!rg.IsValid())
    {
        EnumFlags<RenderGroupFlags> renderGroupFlags = RenderGroupFlags::DEFAULT;

        // Disable occlusion culling for translucent objects
        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        if (rb == RB_TRANSLUCENT || rb == RB_DEBUG)
        {
            renderGroupFlags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
        }

        ShaderDefinition shaderDefinition = attributes.GetShaderDefinition();

        ShaderRef shader = g_shaderManager->GetOrCreate(shaderDefinition);

        if (!shader.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create shader for RenderProxy");

            return;
        }

        // Create RenderGroup
        rg = CreateObject<RenderGroup>(shader, attributes, renderGroupFlags);

        if (renderGroupFlags & RenderGroupFlags::INDIRECT_RENDERING)
        {
            AssertDebug(mapping.indirectRenderer == nullptr, "Indirect renderer already exists on mapping");

            mapping.indirectRenderer = new IndirectRenderer();
            mapping.indirectRenderer->Create(rg->GetDrawCallCollectionImpl());

            mapping.drawCallCollection.impl = rg->GetDrawCallCollectionImpl();
        }

        InitObject(rg);
    }

    mapping.meshProxies.Set(proxy->entity.Id().ToIndex(), proxy);
}

static bool RemoveRenderProxy(RenderProxyList* renderProxyList, ResourceTracker<ObjId<Entity>, RenderProxyMesh>& meshes, RenderProxyMesh* proxy, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    auto& mappings = renderProxyList->mappingsByBucket[rb];

    auto it = mappings.Find(attributes);
    Assert(it != mappings.End());

    DrawCallCollectionMapping& mapping = it->second;
    Assert(mapping.IsValid());

    if (!mapping.meshProxies.HasIndex(proxy->entity.Id().ToIndex()))
    {
        HYP_LOG(Rendering, Warning, "RenderProxyList::RemoveRenderProxy: Render proxy not found in mapping for entity {}", proxy->entity.Id());
        return false;
    }

    mapping.meshProxies.EraseAt(proxy->entity.Id().ToIndex());

    return true;
}

RenderProxyList::RenderProxyList()
    : viewport(Viewport { Vec2u::One(), Vec2i::Zero() }),
      priority(0),
      parallelRenderingStateHead(nullptr),
      parallelRenderingStateTail(nullptr)
{
}

RenderProxyList::~RenderProxyList()
{
    if (parallelRenderingStateHead)
    {
        ParallelRenderingState* state = parallelRenderingStateHead;

        while (state != nullptr)
        {
            if (state->taskBatch != nullptr)
            {
                state->taskBatch->AwaitCompletion();
                delete state->taskBatch;
            }

            ParallelRenderingState* nextState = state->next;

            delete state;

            state = nextState;
        }
    }

    Clear();

#define DO_FINALIZATION_CHECK(tracker)                                                                                                    \
    Assert(tracker.NumCurrent() == 0,                                                                                                     \
        HYP_STR(tracker) " still has %u bits set. This means that there are still render proxies that have not been removed or cleared.", \
        tracker.NumCurrent())

    DO_FINALIZATION_CHECK(meshes);
    DO_FINALIZATION_CHECK(envProbes);
    DO_FINALIZATION_CHECK(envGrids);
    DO_FINALIZATION_CHECK(lights);
    DO_FINALIZATION_CHECK(lightmapVolumes);

#undef DO_FINALIZATION_CHECK
}

void RenderProxyList::Clear()
{
    HYP_SCOPE;

    // Do not fully clear, keep the attribs around so that we can have memory reserved for each slot,
    // as well as render groups.
    for (auto& mappings : mappingsByBucket)
    {
        for (auto& it : mappings)
        {
            DrawCallCollectionMapping& mapping = it.second;
            mapping.meshProxies.Clear();

            if (mapping.indirectRenderer)
            {
                delete mapping.indirectRenderer;
                mapping.indirectRenderer = nullptr;
            }
        }
    }
}

void RenderProxyList::RemoveEmptyRenderGroups()
{
    HYP_SCOPE;

    for (auto& mappings : mappingsByBucket)
    {
        for (auto it = mappings.Begin(); it != mappings.End();)
        {
            DrawCallCollectionMapping& mapping = it->second;
            AssertDebug(mapping.IsValid());

            if (mapping.meshProxies.Any())
            {
                ++it;

                continue;
            }

            if (mapping.indirectRenderer)
            {
                delete mapping.indirectRenderer;
                mapping.indirectRenderer = nullptr;
            }

            it = mappings.Erase(it);
        }
    }
}

uint32 RenderProxyList::NumRenderGroups() const
{
    uint32 count = 0;

    for (const auto& mappings : mappingsByBucket)
    {
        for (const auto& it : mappings)
        {
            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            if (mapping.IsValid())
            {
                ++count;
            }
        }
    }

    return count;
}

void RenderProxyList::BuildRenderGroups(View* view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(~g_renderThread);

    AssertDebug(view != nullptr);
    AssertDebug(view->GetOutputTarget().IsValid());

    // should be in this state - this should be called from the game thread when the render proxy list is being built
    AssertDebug(state == CS_WRITING);

    const RenderableAttributeSet* overrideAttributes = view->GetOverrideAttributes().TryGet();

    auto diff = meshes.GetDiff();

    if (diff.NeedsUpdate())
    {
        Array<RenderProxyMesh*> removedProxies;
        meshes.GetRemoved(removedProxies, true /* includeChanged */);

        Array<RenderProxyMesh*> addedProxyPtrs;
        meshes.GetAdded(addedProxyPtrs, true /* includeChanged */);

        if (addedProxyPtrs.Any() || removedProxies.Any())
        {
            for (RenderProxyMesh* proxy : removedProxies)
            {
                AssertDebug(proxy != nullptr);

                RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, overrideAttributes);
                UpdateRenderableAttributesDynamic(proxy, attributes);

                const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

                Assert(RemoveRenderProxy(this, meshes, proxy, attributes, rb));
            }

            for (RenderProxyMesh* proxy : addedProxyPtrs)
            {
                const Handle<Mesh>& mesh = proxy->mesh;
                Assert(mesh.IsValid());
                Assert(mesh->IsReady());

                const Handle<Material>& material = proxy->material;
                Assert(material.IsValid());

                RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, overrideAttributes);
                UpdateRenderableAttributesDynamic(proxy, attributes);

                const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

                AddRenderProxy(this, meshes, proxy, attributes, rb);
            }
        }
    }
}

ParallelRenderingState* RenderProxyList::AcquireNextParallelRenderingState()
{
    ParallelRenderingState* curr = parallelRenderingStateTail;

    if (!curr)
    {
        if (!parallelRenderingStateHead)
        {
            parallelRenderingStateHead = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch();
            taskBatch->pool = &pool;

            parallelRenderingStateHead->taskBatch = taskBatch;
            parallelRenderingStateHead->numBatches = ParallelRenderingState::maxBatches;
        }

        curr = parallelRenderingStateHead;
    }
    else
    {
        if (!curr->next)
        {
            ParallelRenderingState* newParallelRenderingState = new ParallelRenderingState();

            TaskThreadPool& pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

            TaskBatch* taskBatch = new TaskBatch();
            taskBatch->pool = &pool;

            newParallelRenderingState->taskBatch = taskBatch;
            newParallelRenderingState->numBatches = ParallelRenderingState::maxBatches;

            curr->next = newParallelRenderingState;
        }

        curr = curr->next;
    }

    parallelRenderingStateTail = curr;

    AssertDebug(curr != nullptr);
    AssertDebug(curr->taskBatch != nullptr);
    AssertDebug(curr->taskBatch->IsCompleted());

    return curr;
}

void RenderProxyList::CommitParallelRenderingState(CmdList& outCommandList)
{
    ParallelRenderingState* state = parallelRenderingStateHead;

    while (state)
    {
        AssertDebug(state->taskBatch != nullptr);

        state->taskBatch->AwaitCompletion();

        outCommandList.Concat(std::move(state->baseCommandList));

        // Add command lists to the frame's command list
        for (CmdList& commandList : state->commandLists)
        {
            outCommandList.Concat(std::move(commandList));
        }

        // Add render stats counts to the engine's render stats
        for (EngineRenderStatsCounts& counts : state->renderStatsCounts)
        {
            g_engine->GetRenderStatsCalculator().AddCounts(counts);

            counts = EngineRenderStatsCounts(); // Reset counts after adding for next use
        }

        state->drawCalls.Clear();
        state->drawCallProcs.Clear();
        state->instancedDrawCalls.Clear();
        state->instancedDrawCallProcs.Clear();

        state->taskBatch->ResetState();

        state = state->next;
    }

    parallelRenderingStateTail = nullptr;
}

#pragma endregion RenderProxyList

#pragma region RenderCollector

void RenderCollector::CollectDrawCalls(RenderProxyList& renderProxyList, uint32 bucketBits)
{
    HYP_SCOPE;

    if (bucketBits == 0)
    {
        static constexpr uint32 allBuckets = (1u << RB_MAX) - 1;
        bucketBits = allBuckets; // All buckets
    }

    HYP_MT_CHECK_RW(renderProxyList.dataRaceDetector);

    using IteratorType = FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>::Iterator;
    Array<IteratorType> iterators;

    FOR_EACH_BIT(bucketBits, bitIndex)
    {
        AssertDebug(bitIndex < renderProxyList.mappingsByBucket.Size());

        auto& mappings = renderProxyList.mappingsByBucket[bitIndex];

        if (mappings.Empty())
        {
            continue;
        }

        for (auto& it : mappings)
        {
            iterators.PushBack(&it);
        }
    }

    if (iterators.Empty())
    {
        return;
    }

    std::sort(iterators.Begin(), iterators.End(), [](IteratorType lhs, IteratorType rhs) -> bool
        {
            return lhs->first.GetDrawableLayer() < rhs->first.GetDrawableLayer();
        });

    for (IteratorType it : iterators)
    {
        const RenderableAttributeSet& attributes = it->first;

        DrawCallCollectionMapping& mapping = it->second;
        AssertDebug(mapping.IsValid());

        DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
        drawCallCollection.impl = mapping.renderGroup->GetDrawCallCollectionImpl();
        drawCallCollection.renderGroup = mapping.renderGroup;

        static const bool uniquePerMaterial = g_renderBackend->GetRenderConfig().ShouldCollectUniqueDrawCallPerMaterial();

        DrawCallCollection previousDrawState = std::move(drawCallCollection);

        for (RenderProxyMesh* meshProxy : mapping.meshProxies)
        {
            AssertDebug(meshProxy->mesh.IsValid());
            AssertDebug(meshProxy->mesh->IsReady());

            AssertDebug(meshProxy->material.IsValid());
            AssertDebug(meshProxy->material->IsReady());

            if (meshProxy->instanceData.numInstances == 0)
            {
                continue;
            }

            DrawCallID drawCallId;

            if (uniquePerMaterial)
            {
                drawCallId = DrawCallID(meshProxy->mesh->Id(), meshProxy->material->Id());
            }
            else
            {
                drawCallId = DrawCallID(meshProxy->mesh->Id());
            }

            if (!meshProxy->instanceData.enableAutoInstancing && meshProxy->instanceData.numInstances == 1)
            {
                drawCallCollection.PushRenderProxy(drawCallId, *meshProxy); // NOLINT(bugprone-use-after-move)

                continue;
            }

            EntityInstanceBatch* batch = nullptr;

            // take a batch for reuse if a draw call was using one
            if ((batch = previousDrawState.TakeDrawCallBatch(drawCallId)) != nullptr)
            {
                const uint32 batchIndex = batch->batchIndex;
                AssertDebug(batchIndex != ~0u);

                // Reset it
                *batch = EntityInstanceBatch { batchIndex };

                drawCallCollection.impl->GetEntityInstanceBatchHolder()->MarkDirty(batch->batchIndex);
            }

            drawCallCollection.PushRenderProxyInstanced(batch, drawCallId, *meshProxy);
        }

        // Any draw calls that were not reused from the previous state, clear them out and release batch indices.
        previousDrawState.ResetDrawCalls();
    }
}

void RenderCollector::PerformOcclusionCulling(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a View attached");
    AssertDebug(renderSetup.passData != nullptr, "RenderSetup must have valid PassData to perform occlusion culling");

    HYP_MT_CHECK_RW(renderProxyList.dataRaceDetector);

    static const bool isIndirectRenderingEnabled = g_renderBackend->GetRenderConfig().IsIndirectRenderingEnabled();
    const bool performOcclusionCulling = isIndirectRenderingEnabled && renderSetup.passData->cullData.depthPyramidImageView != nullptr;

    if (performOcclusionCulling)
    {
        FOR_EACH_BIT(bucketBits, bitIndex)
        {
            AssertDebug(bitIndex < renderProxyList.mappingsByBucket.Size());

            auto& mappings = renderProxyList.mappingsByBucket[bitIndex];

            if (mappings.Empty())
            {
                continue;
            }

            for (auto& it : mappings)
            {
                DrawCallCollectionMapping& mapping = it.second;
                AssertDebug(mapping.IsValid());

                const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
                AssertDebug(renderGroup.IsValid());

                DrawCallCollection& drawCallCollection = mapping.drawCallCollection;
                IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

                if (renderGroup->GetFlags() & RenderGroupFlags::OCCLUSION_CULLING)
                {
                    AssertDebug((renderGroup->GetFlags() & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));
                    AssertDebug(indirectRenderer != nullptr);

                    indirectRenderer->GetDrawState().ResetDrawState();

                    indirectRenderer->PushDrawCallsToIndirectState(drawCallCollection);
                    indirectRenderer->ExecuteCullShaderInBatches(frame, renderSetup);
                }
            }
        }
    }
}

void RenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, uint32 bucketBits)
{
    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView(), "RenderSetup must have a View attached");

    if (renderSetup.view->GetView()->GetFlags() & ViewFlags::GBUFFER)
    {
        // Pass NULL framebuffer for GBuffer rendering, as it will be handled by DeferredRenderer outside of this scope.
        ExecuteDrawCalls(frame, renderSetup, renderProxyList, FramebufferRef::Null(), bucketBits);
    }
    else
    {
        const FramebufferRef& framebuffer = renderSetup.view->GetView()->GetOutputTarget().GetFramebuffer();
        AssertDebug(framebuffer != nullptr, "Must have a valid framebuffer for rendering");

        ExecuteDrawCalls(frame, renderSetup, renderProxyList, framebuffer, bucketBits);
    }
}

void RenderCollector::ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, RenderProxyList& renderProxyList, const FramebufferRef& framebuffer, uint32 bucketBits)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    HYP_MT_CHECK_RW(renderProxyList.dataRaceDetector);

    Span<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>> groupsView;

    if (bucketBits == 0)
    {
        static constexpr uint32 allBuckets = (1u << RB_MAX) - 1;
        bucketBits = allBuckets; // All buckets
    }

    // If only one bit is set, we can skip the loop by directly accessing the RenderGroup
    if (ByteUtil::BitCount(bucketBits) == 1)
    {
        const RenderBucket rb = RenderBucket(MathUtil::FastLog2_Pow2(bucketBits));

        auto& mappings = renderProxyList.mappingsByBucket[rb];

        if (mappings.Empty())
        {
            return;
        }

        groupsView = { &mappings, 1 };
    }
    else
    {
        bool allEmpty = true;

        for (const auto& mappings : renderProxyList.mappingsByBucket)
        {
            if (mappings.Any())
            {
                if (AnyOf(mappings, [&bucketBits](const auto& it)
                        {
                            return (bucketBits & (1u << uint32(it.first.GetMaterialAttributes().bucket))) != 0;
                        }))
                {
                    allEmpty = false;

                    break;
                }
            }
        }

        if (allEmpty)
        {
            return;
        }

        groupsView = renderProxyList.mappingsByBucket.ToSpan();
    }

    if (framebuffer)
    {
        frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, frameIndex);
    }

    for (const auto& mappings : groupsView)
    {
        for (const auto& it : mappings)
        {
            const RenderableAttributeSet& attributes = it.first;

            const DrawCallCollectionMapping& mapping = it.second;
            AssertDebug(mapping.IsValid());

            const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

            if (!(bucketBits & (1u << uint32(rb))))
            {
                continue;
            }

            const Handle<RenderGroup>& renderGroup = mapping.renderGroup;
            AssertDebug(renderGroup.IsValid());

            const DrawCallCollection& drawCallCollection = mapping.drawCallCollection;

            IndirectRenderer* indirectRenderer = mapping.indirectRenderer;

            ParallelRenderingState* parallelRenderingState = nullptr;

            if (renderGroup->GetFlags() & RenderGroupFlags::PARALLEL_RENDERING)
            {
                parallelRenderingState = renderProxyList.AcquireNextParallelRenderingState();
            }

            renderGroup->PerformRendering(frame, renderSetup, drawCallCollection, indirectRenderer, parallelRenderingState);

            if (parallelRenderingState != nullptr)
            {
                AssertDebug(parallelRenderingState->taskBatch != nullptr);

                TaskSystem::GetInstance().EnqueueBatch(parallelRenderingState->taskBatch);
            }
        }
    }

    // Wait for all parallel rendering tasks to finish
    renderProxyList.CommitParallelRenderingState(frame->GetCommandList());

    if (framebuffer)
    {
        frame->GetCommandList().Add<EndFramebuffer>(framebuffer, frameIndex);
    }
}

#pragma endregion RenderCollector

} // namespace hyperion