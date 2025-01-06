/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderGroup.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Scene.hpp>
#include <rendering/Camera.hpp>
#include <rendering/Skeleton.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/IndirectDraw.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RenderConfig.hpp>

#include <scene/Entity.hpp>
#include <scene/animation/Skeleton.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/util/ForEach.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>
#include <Constants.hpp>

namespace hyperion {

using renderer::CommandBufferType;

#pragma region Render commands

struct RENDER_COMMAND(CreateGraphicsPipeline) : renderer::RenderCommand
{
    WeakHandle<RenderGroup>             render_group_weak;
    GraphicsPipelineRef                 pipeline;
    RenderPassRef                       render_pass;
    Array<FramebufferRef>               framebuffers;
    RenderGroup::AsyncCommandBuffers    command_buffers;
    RenderableAttributeSet              attributes;

    RENDER_COMMAND(CreateGraphicsPipeline)(
        const WeakHandle<RenderGroup> &render_group_weak,
        const GraphicsPipelineRef &pipeline,
        const RenderPassRef &render_pass,
        const Array<FramebufferRef> &framebuffers,
        const RenderGroup::AsyncCommandBuffers &command_buffers,
        const RenderableAttributeSet &attributes
    ) : render_group_weak(render_group_weak),
        pipeline(pipeline),
        render_pass(render_pass),
        framebuffers(framebuffers),
        command_buffers(command_buffers),
        attributes(attributes)
    {
        AssertThrow(pipeline.IsValid());
    }

    virtual ~RENDER_COMMAND(CreateGraphicsPipeline)() override = default;

    virtual RendererResult operator()() override
    {
#if 0
        DebugLog(LogType::Debug, "Descriptor table has %u sets:\n", pipeline->GetDescriptorTable()->GetSets().Size());

        for (const auto &set : pipeline->GetDescriptorTable()->GetSets()[0]) {
            DebugLog(LogType::Debug, "\t%s %u\n", set->GetLayout().GetName().LookupString(), pipeline->GetDescriptorTable()->GetDescriptorSetIndex(set->GetLayout().GetName()));

            for (const auto &it : set->GetLayout().GetElements()) {
                DebugLog(LogType::Debug, "\t\t%s %u %u\n", it.first.LookupString(), it.second.type, it.second.binding);
            }
        }
#endif

        AssertThrow(pipeline->GetDescriptorTable().IsValid());

        for (auto &it : pipeline->GetDescriptorTable()->GetSets()) {
            for (const auto &set : it) {
                AssertThrow(set.IsValid());
                AssertThrowMsg(set->GetPlatformImpl().GetVkDescriptorSetLayout() != VK_NULL_HANDLE,
                    "Invalid descriptor set for %s", set.GetName().LookupString());
            }
        }

        pipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertex_attributes);
        pipeline->SetTopology(attributes.GetMeshAttributes().topology);
        pipeline->SetCullMode(attributes.GetMaterialAttributes().cull_faces);
        pipeline->SetFillMode(attributes.GetMaterialAttributes().fill_mode);
        pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blend_function);
        pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencil_function);
        pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_TEST));
        pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_WRITE));
        pipeline->SetRenderPass(render_pass);
        pipeline->SetFramebuffers(framebuffers);

        for (uint i = 0; i < max_frames_in_flight; i++) {
            for (uint j = 0; j < uint(command_buffers[i].Size()); j++) {
                if (!command_buffers[i][j].IsValid()) {
                    continue;
                }

#ifdef HYP_VULKAN
                command_buffers[i][j]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[j];
#endif

                HYPERION_BUBBLE_ERRORS(command_buffers[i][j]->Create(g_engine->GetGPUInstance()->GetDevice()));
            }
        }

        HYPERION_BUBBLE_ERRORS(pipeline->Create(g_engine->GetGPUDevice()));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region RenderGroup

RenderGroup::RenderGroup()
    : HypObject(),
      m_flags(RenderGroupFlags::NONE),
      m_pipeline(MakeRenderObject<GraphicsPipeline>()),
      m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
}

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    EnumFlags<RenderGroupFlags> flags
) : HypObject(),
    m_flags(flags),
    m_shader(shader),
    m_pipeline(MakeRenderObject<GraphicsPipeline>()),
    m_renderable_attributes(renderable_attributes),
    m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShader(m_shader);
    }
}

RenderGroup::RenderGroup(
    const ShaderRef &shader,
    const RenderableAttributeSet &renderable_attributes,
    const DescriptorTableRef &descriptor_table,
    EnumFlags<RenderGroupFlags> flags
) : HypObject(),
    m_flags(flags),
    m_pipeline(MakeRenderObject<GraphicsPipeline>(ShaderRef::unset, descriptor_table)),
    m_shader(shader),
    m_renderable_attributes(renderable_attributes),
    m_draw_state(GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>(max_entity_instance_batches), this)
{
    if (m_shader != nullptr) {
        m_pipeline->SetShader(m_shader);
    }
}

RenderGroup::~RenderGroup()
{
    if (m_indirect_renderer != nullptr) {
        m_indirect_renderer->Destroy();
    }
    
    SafeRelease(std::move(m_shader));
    SafeRelease(std::move(m_fbos));

    if (m_command_buffers != nullptr) {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            SafeRelease(std::move((*m_command_buffers)[frame_index]));
        }

        m_command_buffers.Reset();
    }

    SafeRelease(std::move(m_pipeline));
}

void RenderGroup::SetShader(const ShaderRef &shader)
{
    HYP_SCOPE;
    
    SafeRelease(std::move(m_shader));

    m_shader = shader;

    if (m_pipeline.IsValid()) {
        m_pipeline->SetShader(m_shader);
    }
}

void RenderGroup::SetRenderableAttributes(const RenderableAttributeSet &renderable_attributes)
{
    m_renderable_attributes = renderable_attributes;
}

void RenderGroup::AddFramebuffer(const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    
    const auto it = m_fbos.Find(framebuffer);

    if (it != m_fbos.End()) {
        return;
    }

    m_fbos.PushBack(framebuffer);
}

void RenderGroup::RemoveFramebuffer(const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    
    const auto it = m_fbos.Find(framebuffer);

    if (it == m_fbos.End()) {
        return;
    }
    
    m_fbos.Erase(it);
}

void RenderGroup::Init()
{
    HYP_SCOPE;
    
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        HYP_SCOPE;

        if (m_indirect_renderer != nullptr) {
            m_indirect_renderer->Destroy();
        }
        
        SafeRelease(std::move(m_shader));
        SafeRelease(std::move(m_fbos));
        SafeRelease(std::move(m_pipeline));

        if (m_command_buffers != nullptr) {
            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                SafeRelease(std::move((*m_command_buffers)[frame_index]));
            }

            m_command_buffers.Reset();
        }
    }));

    CreateIndirectRenderer();
    CreateCommandBuffers();
    CreateGraphicsPipeline();
        
    SetReady(true);
}

void RenderGroup::CreateIndirectRenderer()
{
    HYP_SCOPE;
    
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer = MakeRefCountedPtr<IndirectRenderer>(&m_draw_state);
        m_indirect_renderer->Create();
    }
}

void RenderGroup::CreateCommandBuffers()
{
    HYP_SCOPE;

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        m_command_buffers = MakeUnique<AsyncCommandBuffers>();

        for (uint i = 0; i < max_frames_in_flight; i++) {
            for (CommandBufferRef &command_buffer : (*m_command_buffers)[i]) {
                command_buffer = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
            }
        }
    }
}

void RenderGroup::CreateGraphicsPipeline()
{
    HYP_SCOPE;

    AssertThrowMsg(m_fbos.Any(), "No framebuffers attached to RenderGroup");

    for (const FramebufferRef &framebuffer : m_fbos) {
        AssertThrow(framebuffer.IsValid());
        
        DeferCreate(framebuffer, g_engine->GetGPUDevice());
    }

    AssertThrow(m_shader.IsValid());

    RenderPassRef render_pass;

    for (const FramebufferRef &framebuffer : m_fbos) {
        if (!render_pass.IsValid()) {
            render_pass = framebuffer->GetRenderPass();
        }
    }

    AssertThrow(m_pipeline.IsValid());
    m_pipeline->SetShader(m_shader);

    if (!m_pipeline->GetDescriptorTable().IsValid()) {
        renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

        DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
        descriptor_table.SetName(CreateNameFromDynamicString(ANSIString("DescriptorTable_") + m_shader->GetCompiledShader()->GetName().LookupString()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint instancing_descriptor_set_index = descriptor_table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancing_descriptor_set_index != ~0u) {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                const GPUBufferRef &gpu_buffer = m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->GetBuffer(frame_index);
                AssertThrow(gpu_buffer.IsValid());

                const DescriptorSetRef &instancing_descriptor_set = descriptor_table->GetDescriptorSet(NAME("Instancing"), frame_index);
                AssertThrow(instancing_descriptor_set.IsValid());

                instancing_descriptor_set->SetElement(NAME("EntityInstanceBatchesBuffer"), gpu_buffer);
            }
        }

        DeferCreate(descriptor_table, g_engine->GetGPUDevice());

        m_pipeline->SetDescriptorTable(descriptor_table);
    }

    AssertThrow(m_pipeline->GetDescriptorTable().IsValid());

    m_pipeline.SetName(CreateNameFromDynamicString(ANSIString("GraphicsPipeline_") + m_shader->GetCompiledShader()->GetName().LookupString()));

    PUSH_RENDER_COMMAND(
        CreateGraphicsPipeline,
        WeakHandleFromThis(),
        m_pipeline,
        render_pass,
        m_fbos,
        m_command_buffers != nullptr ? *m_command_buffers : AsyncCommandBuffers { },
        m_renderable_attributes
    );
}

void RenderGroup::ClearProxies()
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    m_render_proxies.Clear();
}

void RenderGroup::AddRenderProxy(const RenderProxy &render_proxy)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertDebug(render_proxy.mesh.IsValid());
    AssertDebug(render_proxy.mesh->IsReady());

    AssertDebug(render_proxy.material.IsValid());

    m_render_proxies.Set(render_proxy.entity.GetID(), render_proxy);
}

bool RenderGroup::RemoveRenderProxy(ID<Entity> entity)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const auto it = m_render_proxies.Find(entity);

    if (it == m_render_proxies.End()) {
        return false;
    }

    m_render_proxies.Erase(it);

    return true;
}

typename RenderProxyEntityMap::Iterator RenderGroup::RemoveRenderProxy(typename RenderProxyEntityMap::ConstIterator iterator)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    return m_render_proxies.Erase(iterator);
}

void RenderGroup::SetDrawCallCollectionImpl(IDrawCallCollectionImpl *draw_call_collection_impl)
{
    AssertThrow(draw_call_collection_impl != nullptr);

    AssertThrowMsg(!IsInitCalled(), "Cannot use SetDrawCallCollectionImpl() after Init() has been called on RenderGroup; graphics pipeline will have been already created");

    m_draw_state = DrawCallCollection(draw_call_collection_impl, this);
}

void RenderGroup::CollectDrawCalls()
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);

    AssertReady();

    const bool unique_per_material = renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial();

    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer->GetDrawState().ResetDrawState();
    }

    m_divided_draw_calls = { };

    DrawCallCollection previous_draw_state = std::move(m_draw_state);

    for (const auto &it : m_render_proxies) {
        const RenderProxy &render_proxy = it.second;

        AssertDebug(render_proxy.material.IsValid());

        AssertDebug(render_proxy.mesh.IsValid());
        AssertDebugMsg(render_proxy.mesh->IsReady(), "Mesh #%u is not ready", render_proxy.mesh->GetID().Value());

        DrawCallID draw_call_id;

        if (unique_per_material) {
            // @TODO: Rather than using Material ID we could use hashcode of the material,
            // so that we can use the same material with different IDs
            draw_call_id = DrawCallID(render_proxy.mesh.GetID(), render_proxy.material.GetID());
        } else {
            draw_call_id = DrawCallID(render_proxy.mesh.GetID());
        }

        EntityInstanceBatch *batch = nullptr;

        // take a batch for reuse if a draw call was using one
        if ((batch = previous_draw_state.TakeDrawCallBatch(draw_call_id)) != nullptr) {
            const uint32 batch_index = batch->batch_index;
            AssertDebug(batch_index != ~0u);

            // Reset it
            *batch = EntityInstanceBatch { batch_index };

            m_draw_state.GetImpl()->GetEntityInstanceBatchHolder()->MarkDirty(batch->batch_index);
        }

        m_draw_state.PushDrawCallToBatch(batch, draw_call_id, render_proxy);
    }

    previous_draw_state.ResetDrawCalls();

    // register draw calls for indirect rendering
    if (m_flags & RenderGroupFlags::INDIRECT_RENDERING) {
        m_indirect_renderer->PushDrawCallsToIndirectState();
    }
}

void RenderGroup::PerformOcclusionCulling(Frame *frame, const CullData *cull_data)
{
    HYP_SCOPE;

    if constexpr (!use_draw_indirect) {
        return;
    }

    AssertThrow((m_flags & (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING)) == (RenderGroupFlags::INDIRECT_RENDERING | RenderGroupFlags::OCCLUSION_CULLING));

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(cull_data != nullptr);

    m_indirect_renderer->ExecuteCullShaderInBatches(
        frame,
        *cull_data
    );
}

static void GetDividedDrawCalls(Span<const DrawCall> draw_calls, uint32 num_batches, Array<Span<const DrawCall>> &out_divided_draw_calls)
{
    HYP_SCOPE;

    const uint32 num_draw_calls = uint32(draw_calls.Size());

    // Make sure we don't try to divide into more batches than we have draw calls
    num_batches = MathUtil::Min(num_batches, num_draw_calls);
    out_divided_draw_calls.Resize(num_batches);

    const uint32 num_draw_calls_divided = (num_draw_calls + num_batches - 1) / num_batches;

    uint32 draw_call_index = 0;

    for (uint32 container_index = 0; container_index < num_batches; container_index++) {
        const uint32 diff_to_next_or_end = MathUtil::Min(num_draw_calls_divided, num_draw_calls - draw_call_index);

        out_divided_draw_calls[container_index] = {
            draw_calls.Begin() + draw_call_index,
            draw_calls.Begin() + draw_call_index + diff_to_next_or_end
        };

        // sanity check
        AssertThrow(draw_calls.Size() >= draw_call_index + diff_to_next_or_end);

        draw_call_index += diff_to_next_or_end;
    }
}


template <bool IsIndirect>
static HYP_FORCE_INLINE void RenderAll(
    Frame *frame,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    const DrawCallCollection &draw_state
)
{
    HYP_SCOPE;

    static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    if (draw_state.GetDrawCalls().Empty()) {
        return;
    }

    const ID<Scene> scene_id = g_engine->GetRenderState()->GetScene().id;

    const CameraRenderResources &camera_render_resources = g_engine->GetRenderState()->GetActiveCamera();
    const uint32 camera_index = camera_render_resources.GetBufferIndex();
    AssertThrow(camera_index != ~0u);

    const uint frame_index = frame->GetFrameIndex();

    TaskThreadPool &pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

    const uint num_batches = use_parallel_rendering
        ? MathUtil::Min(pool.NumThreads(), num_async_rendering_command_buffers)
        : 1u;

    const uint global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    const uint instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
    const DescriptorSetRef &instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);
    
    for (const DrawCall &draw_call : draw_state.GetDrawCalls()) {
        EntityInstanceBatch *batch = draw_call.batch;
        AssertDebug(batch != nullptr);

        pipeline->Bind(frame->GetCommandBuffer());

        global_descriptor_set->Bind(
            frame->GetCommandBuffer(),
            pipeline,
            { },
            global_descriptor_set_index
        );

        scene_descriptor_set->Bind(
            frame->GetCommandBuffer(),
            pipeline,
            {
                { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(g_engine->GetRenderState()->GetScene().id.ToIndex()) },
                { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_index) },
                { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
            },
            scene_descriptor_set_index
        );

        // Bind textures globally (bindless)
        if (material_descriptor_set_index != ~0u && use_bindless_textures) {
            material_descriptor_set->Bind(frame->GetCommandBuffer(), pipeline, material_descriptor_set_index);
        }

        // @TODO Use EntityInstanceBatch at index 0 for when we don't need an EntityInstanceBatch (only one instance)
        // we'll keep that one with matrix set to identity

        if (entity_descriptor_set.IsValid()) {
            if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
                entity_descriptor_set->Bind(
                    frame->GetCommandBuffer(),
                    pipeline,
                    {
                        { NAME("MaterialsBuffer"), ShaderDataOffset<MaterialShaderData>(draw_call.material != nullptr ? draw_call.material->GetRenderResources().GetBufferIndex() : 0) },
                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton != nullptr ? draw_call.skeleton->GetRenderResources().GetBufferIndex() : 0) }
                    },
                    entity_descriptor_set_index
                );
            } else {
                entity_descriptor_set->Bind(
                    frame->GetCommandBuffer(),
                    pipeline,
                    {
                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton != nullptr ? draw_call.skeleton->GetRenderResources().GetBufferIndex() : 0) }
                    },
                    entity_descriptor_set_index
                );
            }
        }

        if (instancing_descriptor_set.IsValid()) {
            instancing_descriptor_set->Bind(
                frame->GetCommandBuffer(),
                pipeline,
                {
                    { NAME("EntityInstanceBatchesBuffer"), uint32(batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf()) }
                },
                instancing_descriptor_set_index
            );
        }

        // Bind material descriptor set
        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
            const DescriptorSetRef &material_descriptor_set = draw_call.material->GetRenderResources().GetDescriptorSets()[frame_index];
            AssertThrow(material_descriptor_set.IsValid());

            material_descriptor_set->Bind(frame->GetCommandBuffer(), pipeline, material_descriptor_set_index);
        }

        if constexpr (IsIndirect) {
            draw_call.mesh->RenderIndirect(
                frame->GetCommandBuffer(),
                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
            );
        } else {
            draw_call.mesh->Render(frame->GetCommandBuffer(), batch->num_entities);
        }
    }
}

template <bool IsIndirect>
static HYP_FORCE_INLINE void RenderAll_Parallel(
    Frame *frame,
    const RenderGroup::AsyncCommandBuffers &command_buffers,
    uint &command_buffer_index,
    const GraphicsPipelineRef &pipeline,
    IndirectRenderer *indirect_renderer,
    Array<Span<const DrawCall>> &divided_draw_calls,
    const DrawCallCollection &draw_state
)
{
    HYP_SCOPE;

    static const bool use_bindless_textures = g_engine->GetGPUDevice()->GetFeatures().SupportsBindlessTextures();

    if (draw_state.GetDrawCalls().Empty()) {
        return;
    }

    const ID<Scene> scene_id = g_engine->GetRenderState()->GetScene().id;

    const uint frame_index = frame->GetFrameIndex();

    TaskThreadPool &pool = TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_RENDER);

    const uint num_batches = MathUtil::Min(pool.NumThreads(), num_async_rendering_command_buffers);
    
    GetDividedDrawCalls(draw_state.GetDrawCalls().ToSpan(), num_async_rendering_command_buffers, divided_draw_calls);

    // rather than using a single integer, we have to set states in a fixed array
    // because otherwise we'd need to use an atomic integer
    FixedArray<uint, num_async_rendering_command_buffers> command_buffers_recorded_states { };

    const uint global_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Global"));
    const DescriptorSetRef &global_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index);

    const uint scene_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));
    const DescriptorSetRef &scene_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index);
    
    const uint material_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Material"));
    const DescriptorSetRef &material_descriptor_set = use_bindless_textures
        ? pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Material"), frame_index)
        : DescriptorSetRef::unset;
    
    const uint entity_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Object"));
    const DescriptorSetRef &entity_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Object"), frame_index);

    const uint instancing_descriptor_set_index = pipeline->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Instancing"));
    const DescriptorSetRef &instancing_descriptor_set = pipeline->GetDescriptorTable()->GetDescriptorSet(NAME("Instancing"), frame_index);

    // AtomicVar<uint32> num_rendered_objects { 0u };
    
    const CameraRenderResources &camera_render_resources = g_engine->GetRenderState()->GetActiveCamera();
    const uint32 camera_index = camera_render_resources.GetBufferIndex();
    AssertThrow(camera_index != ~0u);

    if (divided_draw_calls.Size() == 1) {
        RenderAll<IsIndirect>(
            frame,
            pipeline,
            indirect_renderer,
            draw_state
        );

        return;
    }

    // HYP_LOG(Rendering, LogLevel::DEBUG, "Rendering {} draw calls in {} batches", draw_state.GetDrawCalls().Size(), num_batches);

    ParallelForEach(divided_draw_calls, num_batches, pool,
        [&](Span<const DrawCall> draw_calls, uint index, uint)
        {
            if (!draw_calls) {
                return;
            }

            command_buffers[frame_index][index]->Record(
                g_engine->GetGPUDevice(),
                pipeline->GetRenderPass(),
                [&](CommandBuffer *secondary)
                {
                    pipeline->Bind(secondary);
                    
                    global_descriptor_set->Bind(
                        secondary,
                        pipeline,
                        { },
                        global_descriptor_set_index
                    );

                    scene_descriptor_set->Bind(
                        secondary,
                        pipeline,
                        {
                            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(g_engine->GetRenderState()->GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_index) },
                            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(g_engine->GetRenderState()->bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(g_engine->GetRenderState()->GetActiveEnvProbe().ToIndex()) }
                        },
                        scene_descriptor_set_index
                    );

                    // Bind textures globally (bindless)
                    if (material_descriptor_set_index != ~0u && use_bindless_textures) {
                        material_descriptor_set->Bind(secondary, pipeline, material_descriptor_set_index);
                    }

                    for (const DrawCall &draw_call : draw_calls) {
                        EntityInstanceBatch *entity_instance_batch = draw_call.batch;
                        AssertDebug(entity_instance_batch != nullptr);

                        if (entity_descriptor_set.IsValid()) {
                            if (renderer::RenderConfig::ShouldCollectUniqueDrawCallPerMaterial()) {
                                entity_descriptor_set->Bind(
                                    secondary,
                                    pipeline,
                                    {
                                        { NAME("MaterialsBuffer"), ShaderDataOffset<MaterialShaderData>(draw_call.material != nullptr ? draw_call.material->GetRenderResources().GetBufferIndex() : 0) },
                                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton != nullptr ? draw_call.skeleton->GetRenderResources().GetBufferIndex() : 0) }
                                    },
                                    entity_descriptor_set_index
                                );
                            } else {
                                entity_descriptor_set->Bind(
                                    secondary,
                                    pipeline,
                                    {
                                        { NAME("SkeletonsBuffer"), ShaderDataOffset<SkeletonShaderData>(draw_call.skeleton != nullptr ? draw_call.skeleton->GetRenderResources().GetBufferIndex() : 0) }
                                    },
                                    entity_descriptor_set_index
                                );
                            }
                        }

                        if (instancing_descriptor_set.IsValid()) {
                            instancing_descriptor_set->Bind(
                                secondary,
                                pipeline,
                                {
                                    { NAME("EntityInstanceBatchesBuffer"), uint32(entity_instance_batch->batch_index * draw_state.GetImpl()->GetBatchSizeOf()) }
                                },
                                instancing_descriptor_set_index
                            );
                        }

                        // Bind material descriptor set
                        if (material_descriptor_set_index != ~0u && !use_bindless_textures) {
                            const DescriptorSetRef &material_descriptor_set = draw_call.material->GetRenderResources().GetDescriptorSets()[frame_index];
                            AssertThrow(material_descriptor_set.IsValid());

                            material_descriptor_set->Bind(secondary, pipeline, material_descriptor_set_index);
                        }

                        if constexpr (IsIndirect) {
                            draw_call.mesh->RenderIndirect(
                                secondary,
                                indirect_renderer->GetDrawState().GetIndirectBuffer(frame_index).Get(),
                                draw_call.draw_command_index * uint32(sizeof(IndirectDrawCommand))
                            );
                        } else {
                            draw_call.mesh->Render(secondary, entity_instance_batch->num_entities);
                        }
                    }

                    HYPERION_RETURN_OK;
                }
            );

            command_buffers_recorded_states[index] = 1u;
        }
    );
    

    const uint num_recorded_command_buffers = command_buffers_recorded_states.Sum();

    // submit all command buffers
    for (uint i = 0; i < num_recorded_command_buffers; i++) {
        command_buffers[frame_index][i]
            ->SubmitSecondary(frame->GetCommandBuffer());
    }

    command_buffer_index = (command_buffer_index + num_recorded_command_buffers) % uint(command_buffers.Size());
}

void RenderGroup::PerformRendering(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    if (m_draw_state.GetDrawCalls().Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        AssertThrow(m_command_buffers != nullptr);

        RenderAll_Parallel<false>(
            frame,
            *m_command_buffers,
            m_command_buffer_index,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_divided_draw_calls,
            m_draw_state
        );
    } else {
        RenderAll<false>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_draw_state
        );
    }
}

void RenderGroup::PerformRenderingIndirect(Frame *frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_RENDER);
    AssertReady();

    AssertThrow(m_flags & RenderGroupFlags::INDIRECT_RENDERING);

    if (m_draw_state.GetDrawCalls().Empty()) {
        return;
    }

    if (m_flags & RenderGroupFlags::PARALLEL_RENDERING) {
        AssertThrow(m_command_buffers != nullptr);

        RenderAll_Parallel<true>(
            frame,
            *m_command_buffers,
            m_command_buffer_index,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_divided_draw_calls,
            m_draw_state
        );
    } else {
        RenderAll<true>(
            frame,
            m_pipeline,
            m_indirect_renderer.Get(),
            m_draw_state
        );
    }
}

#pragma endregion RenderGroup

#pragma region RendererProxy


#pragma endregion RendererProxy

} // namespace hyperion
