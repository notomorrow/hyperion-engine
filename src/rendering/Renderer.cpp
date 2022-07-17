#include "Renderer.hpp"
#include <Engine.hpp>
#include <Constants.hpp>

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

namespace hyperion::v2 {

IndirectRenderer::IndirectRenderer()
{
}

IndirectRenderer::~IndirectRenderer()
{
}

void IndirectRenderer::RebuildDescriptors(Engine *engine, Frame *frame)
{
    const auto frame_index = frame->GetFrameIndex();

    auto &descriptor_set = m_descriptor_sets[frame_index];

    descriptor_set->GetDescriptor(3)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(3)->SetSubDescriptor({
        .element_index = 0,
        .buffer        = m_indirect_draw_state.GetInstanceBuffer(frame_index)
    });

    descriptor_set->GetDescriptor(4)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(4)->SetSubDescriptor({
        .element_index = 0,
        .buffer        = m_indirect_draw_state.GetIndirectBuffer(frame_index)
    });

    descriptor_set->ApplyUpdates(engine->GetDevice());
}

void IndirectRenderer::Create(Engine *engine)
{
    HYPERION_ASSERT_RESULT(m_indirect_draw_state.Create(engine));

    const IndirectParams initial_params{};

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        HYPERION_ASSERT_RESULT(m_indirect_params_buffers[frame_index].Create(
            engine->GetDevice(),
            sizeof(IndirectParams)
        ));

        m_indirect_params_buffers[frame_index].Copy(
            engine->GetDevice(),
            sizeof(IndirectParams),
            &initial_params
        );

        m_descriptor_sets[frame_index] = std::make_unique<DescriptorSet>();

        // global object data
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(0)
            ->SetSubDescriptor({
                .buffer = engine->shader_globals->objects.GetBuffers()[frame_index].get()
            });

        // global scene data
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(1)
            ->SetSubDescriptor({
                .buffer = engine->shader_globals->scenes.GetBuffers()[frame_index].get()
            });

        // params buffer
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::UniformBufferDescriptor>(2)
            ->SetSubDescriptor({
                .buffer = &m_indirect_params_buffers[frame_index]
            });

        // instances buffer
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
            ->SetSubDescriptor({
                .buffer = m_indirect_draw_state.GetInstanceBuffer(frame_index)
            });

        // indirect commands
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(4)
            ->SetSubDescriptor({
                .buffer = m_indirect_draw_state.GetIndirectBuffer(frame_index)
            });

        HYPERION_ASSERT_RESULT(m_descriptor_sets[frame_index]->Create(
            engine->GetDevice(),
            &engine->GetInstance()->GetDescriptorPool()
        ));
    }

    // create compute pipeline for object visibility (for indirect render)
    // TODO: cache pipelines: re-use this
    m_object_visibility = engine->resources.compute_pipelines.Add(std::make_unique<ComputePipeline>(
        engine->resources.shaders.Add(std::make_unique<Shader>(
            std::vector<SubShader>{
                { ShaderModule::Type::COMPUTE, {FileByteReader(FileSystem::Join(engine->assets.GetBasePath(), "vkshaders/cull/object_visibility.comp.spv")).Read()}}
            }
        )),
        DynArray<const DescriptorSet *> { m_descriptor_sets[0].get() }
    ));

    m_object_visibility.Init();
}

void IndirectRenderer::Destroy(Engine *engine)
{
    for (auto &params_buffer : m_indirect_params_buffers) {
        HYPERION_ASSERT_RESULT(params_buffer.Destroy(engine->GetDevice()));
    }

    for (auto &descriptor_set : m_descriptor_sets) {
        HYPERION_ASSERT_RESULT(descriptor_set->Destroy(engine->GetDevice()));
    }

    m_object_visibility.Reset();

    HYPERION_ASSERT_RESULT(m_indirect_draw_state.Destroy(engine));
}

void IndirectRenderer::ExecuteCullShaderInBatches(Engine *engine, Frame *frame)
{
    static constexpr UInt batch_size = 256u;

    auto *command_buffer     = frame->GetCommandBuffer();
    const auto frame_index   = frame->GetFrameIndex();

    const UInt num_drawables = static_cast<UInt>(m_indirect_draw_state.GetDrawables().Size());
    const UInt num_batches   = (num_drawables / batch_size) + 1;

    if (num_drawables == 0) {
        return;
    }

    bool was_buffer_resized = false;
    m_indirect_draw_state.UpdateBufferData(engine, frame, &was_buffer_resized);

    if (was_buffer_resized) {
        RebuildDescriptors(engine, frame);
    }

    const auto scene_id = engine->render_state.GetScene().id;

    // bind our descriptor set to binding point 0
    command_buffer->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_object_visibility->GetPipeline(),
        m_descriptor_sets[frame_index].get(),
        static_cast<DescriptorSet::Index>(0)
    );

    UInt count_remaining = static_cast<UInt>(num_drawables);

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        const UInt num_drawables_in_batch = MathUtil::Min(count_remaining, batch_size);

        m_object_visibility->GetPipeline()->Bind(command_buffer, Pipeline::PushConstantData {
            .object_visibility_data = {
                .batch_offset  = batch_index * batch_size,
                .num_drawables = num_drawables_in_batch,
                .scene_id      = static_cast<UInt32>(scene_id.value)
            }
        });

        m_object_visibility->GetPipeline()->Dispatch(command_buffer, Extent3D { 1, 1, 1 });

        count_remaining -= num_drawables_in_batch;
    }
}

GraphicsPipeline::GraphicsPipeline(
    Ref<Shader> &&shader,
    Ref<RenderPass> &&render_pass,
    const RenderableAttributeSet &renderable_attributes
) : EngineComponentBase(),
    m_pipeline(std::make_unique<renderer::GraphicsPipeline>()),
    m_shader(std::move(shader)),
    m_render_pass(std::move(render_pass)),
    m_renderable_attributes(renderable_attributes),
    m_per_frame_data(nullptr),
    m_multiview_index(~0u)
{
}

GraphicsPipeline::~GraphicsPipeline()
{
    Teardown();
}

void GraphicsPipeline::RemoveFramebuffer(Framebuffer::ID id)
{
    const auto it = std::find_if(
        m_fbos.begin(),
        m_fbos.end(),
        [&](const auto &item) {
            return item->GetId() == id;
        }
    );

    if (it == m_fbos.end()) {
        return;
    }
    
    m_fbos.erase(it);
}

void GraphicsPipeline::AddSpatial(Ref<Spatial> &&spatial)
{
    AssertThrow(spatial != nullptr);
    // AssertThrow(spatial->IsReady());

    // AssertThrow(spatial->IsRenderable());

    // FIXME: thread safety. this could be called from any thread
    AssertThrowMsg(
        (m_renderable_attributes.vertex_attributes & spatial->GetRenderableAttributes().vertex_attributes) == spatial->GetRenderableAttributes().vertex_attributes,
        "Pipeline vertex attributes does not satisfy the required vertex attributes of the spatial."
    );

    // if (IsInitCalled()) {
    //     spatial->Init(GetEngine());
    // }

    spatial->OnAddedToPipeline(this);
    
    std::lock_guard guard(m_enqueued_spatials_mutex);

    // remove from 'pending removal' list
    auto it = std::find(
        m_spatials_pending_removal.begin(),
        m_spatials_pending_removal.end(),
        spatial
    );

    if (it != m_spatials_pending_removal.end()) {
        m_spatials_pending_removal.erase(it);
    }
    
    m_spatials_pending_addition.push_back(std::move(spatial));
    
    UpdateEnqueuedSpatialsFlag();
}

void GraphicsPipeline::RemoveSpatial(Ref<Spatial> &&spatial, bool call_on_removed)
{
    // TODO:: Make all GraphicsPipeline operations operate in the RENDER
    // thread. Deferred rendering will be some derived version of a RenderComponent, so
    // it will acquire spatials that way and hand them off from the render thread here.

    // we cannot touch m_spatials from any thread other than the render thread
    // we'll have to assume it's here, and check later :/ 

    // auto spatials_it = std::find_if(
    //     m_spatials.begin(),
    //     m_spatials.end(),
    //     [&id](const auto &item) {
    //         AssertThrow(item != nullptr);
    //         return item->GetId() == id;
    //     }
    // );

    // if (spatials_it != m_spatials.end()) {
    //     auto &&found_spatial = *spatials_it;


        // add it to pending removal list
        std::lock_guard guard(m_enqueued_spatials_mutex);

        const auto pending_removal_it = std::find(
            m_spatials_pending_removal.begin(),
            m_spatials_pending_removal.end(),
            spatial
        );

        if (pending_removal_it == m_spatials_pending_removal.end()) {
            if (call_on_removed) {
                spatial->OnRemovedFromPipeline(this);
            }

            auto pending_addition_it = std::find(
                m_spatials_pending_addition.begin(),
                m_spatials_pending_addition.end(),
                spatial
            );

            if (pending_addition_it != m_spatials_pending_addition.end()) {
                
                // directly remove from list of ones pending addition
                m_spatials_pending_addition.erase(pending_addition_it);
            } else {
                /*m_cached_render_data.push_back(CachedRenderData{
                    .cycles_remaining = max_frames_in_flight + 1,
                    .spatial_id       = spatial->GetId(),
                    .material = spatial->GetMaterial() != nullptr
                        ? spatial->GetMaterial().IncRef()
                        : nullptr,
                    .mesh     = spatial->GetMesh() != nullptr
                        ? spatial->GetMesh().IncRef()
                        : nullptr,
                    .skeleton = spatial->GetSkeleton() != nullptr
                        ? spatial->GetSkeleton().IncRef()
                        : nullptr,
                    .shader   = spatial->GetShader() != nullptr
                        ? spatial->GetShader().IncRef()
                        : nullptr
                });*/

                m_spatials_pending_removal.push_back(std::move(spatial));
            }

            UpdateEnqueuedSpatialsFlag();
        } else {
            DebugLog(
                LogType::Info,
                "Spatial #%u is already pending removal from pipeline #%u\n",
                spatial->GetId().value,
                GetId().value
            );
        }

        // return;
    // }

    // not found in main spatials list, check pending addition/removal

    // std::lock_guard guard(m_enqueued_spatials_mutex);


    
}

// void GraphicsPipeline::OnSpatialRemoved(Spatial *spatial)
// {
//     RemoveSpatial(spatial->GetId(), false);
// }

void GraphicsPipeline::BuildDrawCommandsBuffer(Engine *engine, UInt frame_index)
{
}

void GraphicsPipeline::PerformEnqueuedSpatialUpdates(Engine *engine, UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    std::lock_guard guard(m_enqueued_spatials_mutex);

    if (!m_spatials_pending_removal.empty()) {
        for (auto it = m_spatials_pending_removal.begin(); it != m_spatials_pending_removal.end();) {
            const auto spatials_it = std::find(m_spatials.begin(), m_spatials.end(), *it); // oof
            
            if (spatials_it != m_spatials.end()) {
                m_spatials.erase(spatials_it);
            }

            it = m_spatials_pending_removal.erase(it);
        }
    }

    if (!m_spatials_pending_addition.empty()) {
        // we only add spatials that are fully ready,
        // keeping ones that aren't finished initializing in the vector,
        // they should be finished on the next pass
        
        auto it = m_spatials_pending_addition.begin();
        
        while (it != m_spatials_pending_addition.end()) {
            AssertThrow(*it != nullptr);

            if ((*it)->GetMesh() == nullptr // TODO: I don't believe it's threadsafe to just check if this is null like this
                || std::find(m_spatials.begin(), m_spatials.end(), *it) != m_spatials.end()) { // :(
                it = m_spatials_pending_addition.erase(it);
                
                continue;
            }
            
            if ((*it)->IsReady() && (*it)->GetMesh()->IsReady()) {
                m_spatials.push_back(std::move(*it));
                it = m_spatials_pending_addition.erase(it);
                
                continue;
            }
            
            // not ready, keep looping.
            ++it;
        }
    }

    UpdateEnqueuedSpatialsFlag();
}

void GraphicsPipeline::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_GRAPHICS_PIPELINES, [this](...) {
        auto *engine = GetEngine();

        AssertThrow(!m_fbos.empty());

        for (auto &fbo : m_fbos) {
            AssertThrow(fbo != nullptr);
            fbo.Init();
        }

        AssertThrow(m_shader != nullptr);
        m_shader.Init();

        for (auto &&spatial : m_spatials) {
            AssertThrow(spatial != nullptr);

            spatial->Init(engine);
        }

        m_indirect_renderer.Create(engine);

        engine->render_scheduler.Enqueue([this, engine](...) {
            renderer::GraphicsPipeline::ConstructionInfo construction_info {
                .vertex_attributes = m_renderable_attributes.vertex_attributes,
                .topology          = m_renderable_attributes.topology,
                .cull_mode         = m_renderable_attributes.cull_faces,
                .fill_mode         = m_renderable_attributes.fill_mode,
                .depth_test        = m_renderable_attributes.depth_test,
                .depth_write       = m_renderable_attributes.depth_write,
                .blend_enabled     = m_renderable_attributes.alpha_blending,
                .shader            = m_shader->GetShaderProgram(),
                .render_pass       = &m_render_pass->GetRenderPass(),
                .stencil_state     = m_renderable_attributes.stencil_state,
                .multiview_index   = m_multiview_index
            };

            for (auto &fbo : m_fbos) {
                construction_info.fbos.push_back(&fbo->GetFramebuffer());
            }

            AssertThrow(m_per_frame_data == nullptr);
            m_per_frame_data = new PerFrameData<CommandBuffer>(engine->GetInstance()->GetFrameHandler()->NumFrames());

            auto &per_frame_data = *m_per_frame_data;

            for (UInt32 i = 0; i < per_frame_data.NumFrames(); i++) {
                auto command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_SECONDARY);
                HYPERION_BUBBLE_ERRORS(command_buffer->Create(
                    engine->GetInstance()->GetDevice(),
                    engine->GetInstance()->GetGraphicsCommandPool()
                ));

                per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
            }

            HYPERION_BUBBLE_ERRORS(m_pipeline->Create(
                engine->GetDevice(),
                std::move(construction_info),
                &engine->GetInstance()->GetDescriptorPool()
            ));
            
            SetReady(true);
            
            HYPERION_RETURN_OK;
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_GRAPHICS_PIPELINES, [this](...) {
            auto *engine = GetEngine();

            SetReady(false);

            m_indirect_renderer.Destroy(engine);

            for (auto &&spatial : m_spatials) {
                AssertThrow(spatial != nullptr);

                spatial->OnRemovedFromPipeline(this);
            }

            m_spatials.clear();
            
            std::lock_guard guard(m_enqueued_spatials_mutex);

            for (auto &&spatial : m_spatials_pending_addition) {
                if (spatial == nullptr) {
                    continue;
                }

                spatial->OnRemovedFromPipeline(this);
            }

            m_spatials_pending_addition.clear();
            m_spatials_pending_removal.clear();

            m_shader.Reset();

            engine->render_scheduler.Enqueue([this, engine](...) {
                if (m_per_frame_data != nullptr) {
                    auto &per_frame_data = *m_per_frame_data;

                    for (UInt i = 0; i < per_frame_data.NumFrames(); i++) {
                        HYPERION_BUBBLE_ERRORS(per_frame_data[i].Get<CommandBuffer>()->Destroy(
                            engine->GetInstance()->GetDevice(),
                            engine->GetInstance()->GetGraphicsCommandPool()
                        ));
                    }

                    delete m_per_frame_data;
                    m_per_frame_data = nullptr;
                }

                return m_pipeline->Destroy(engine->GetDevice());
            });
            
            HYP_FLUSH_RENDER_QUEUE(engine);
        }));
    }));
}

void GraphicsPipeline::CollectDrawCalls(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (m_enqueued_spatials_flag.load()) {
        PerformEnqueuedSpatialUpdates(engine, frame->GetFrameIndex());
    }

    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_id      = scene_binding.id;
    const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

    // check visibility state
    const bool perform_culling = scene_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.bucket);

    m_indirect_renderer.GetDrawState().ResetDrawables();

    for (auto &&spatial : m_spatials) {
        if (spatial->GetMesh() == nullptr) {
            continue;
        }

        if (perform_culling) {
            if (auto *octant = spatial->GetOctree()) {
                const auto &visibility_state = octant->GetVisibilityState();

                if (!Octree::IsVisible(&engine->GetWorld().GetOctree(), octant)) {
                    continue;
                }

                if (!visibility_state.Get(scene_id)) {
                    continue;
                }
            } else {
                continue;
            }
        }

        m_indirect_renderer.GetDrawState().PushDrawable(Drawable(spatial->GetDrawable()));
    }

    m_indirect_renderer.ExecuteCullShaderInBatches(engine, frame);
}

void GraphicsPipeline::PerformRendering(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_per_frame_data != nullptr);

    const auto scene_binding = engine->render_state.GetScene();
    const auto scene_id      = scene_binding.id;

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_pipeline->GetConstructionInfo().render_pass,
        [this, engine, instance, device, scene_id, frame_index = frame->GetFrameIndex()](CommandBuffer *secondary) {    
            UInt num_culled_objects = 0;
   
            m_pipeline->Bind(secondary);

            secondary->BindDescriptorSets(
                instance->GetDescriptorPool(),
                m_pipeline.get(),
                FixedArray { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32((scene_id.value - 1) * sizeof(SceneShaderData)),
                    UInt32(0           * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
#endif
            
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
                }
            );

            for (Drawable &drawable : m_indirect_renderer.GetDrawState().GetDrawables()) {
                const UInt entity_index = drawable.entity_id != Spatial::empty_id
                    ? drawable.entity_id.value - 1
                    : 0;

                const UInt skeleton_index = drawable.skeleton_id != Skeleton::empty_id
                    ? drawable.skeleton_id.value - 1
                    : 0;

                const UInt material_index = drawable.material_id != Material::empty_id
                    ? drawable.material_id.value - 1
                    : 0;

#if HYP_FEATURES_BINDLESS_TEXTURES
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index   * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT,        DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(entity_index   * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#endif

                drawable.mesh->RenderIndirect(
                    engine,
                    secondary,
                    m_indirect_renderer.GetDrawState().GetIndirectBuffer(frame_index),
                    drawable.object_instance.draw_command_index * sizeof(IndirectDrawCommand)
                );
            }

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}


void GraphicsPipeline::Render(Engine *engine, Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    AssertThrow(m_per_frame_data != nullptr);

    /* Remove cache data. This data exists so that there is some buffer time
       in between removing an entity from the pipeline and the data (potentially)
       being destroyed. We use (max frames in flight) + 1 and count down each Render() call.
    */
    
    /*if (!m_cached_render_data.empty()) {
        auto it = m_cached_render_data.begin();

        while (it != m_cached_render_data.end()) {
            if (!--it->cycles_remaining) {
                it = m_cached_render_data.erase(it);
            } else {
                ++it;
            }
        }
    }*/

    if (m_enqueued_spatials_flag.load()) {
        PerformEnqueuedSpatialUpdates(engine, frame->GetFrameIndex());
    }

    auto *instance = engine->GetInstance();
    auto *device = instance->GetDevice();
    auto *secondary_command_buffer = m_per_frame_data->At(frame->GetFrameIndex()).Get<CommandBuffer>();

    secondary_command_buffer->Record(
        device,
        m_pipeline->GetConstructionInfo().render_pass,
        [this, engine, instance, device, frame_index = frame->GetFrameIndex()](CommandBuffer *secondary) {    
            UInt num_culled_objects = 0;
   
            m_pipeline->Bind(secondary);

            static_assert(std::size(DescriptorSet::object_buffer_mapping) == max_frames_in_flight);

            const auto scene_binding = engine->render_state.GetScene();
            const auto scene_cull_id = scene_binding.id; //scene_binding.parent_id ? scene_binding.parent_id : scene_binding.id;
            const auto scene_index   = scene_binding ? scene_binding.id.value - 1 : 0;

            secondary->BindDescriptorSets(
                instance->GetDescriptorPool(),
                m_pipeline.get(),
                FixedArray { DescriptorSet::global_buffer_mapping[frame_index], DescriptorSet::scene_buffer_mapping[frame_index] },
                FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,        DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE },
                FixedArray {
                    UInt32(scene_index * sizeof(SceneShaderData)),
                    UInt32(0           * sizeof(LightShaderData))
                }
            );

#if HYP_FEATURES_BINDLESS_TEXTURES
            /* Bindless textures */
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::bindless_textures_mapping[frame_index], .count = 1},
                    {.binding = DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS}
                }
            );
#endif
            
            instance->GetDescriptorPool().Bind(
                device,
                secondary,
                m_pipeline.get(),
                {
                    {.set = DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER, .count = 1},
                }
            );

            // check visibility state
            const bool perform_culling = scene_cull_id != Scene::empty_id && BucketFrustumCullingEnabled(m_renderable_attributes.bucket);
            
            for (auto &&spatial : m_spatials) {
                if (perform_culling) {
                    if (auto *octant = spatial->GetOctree()) {
                        const auto &visibility_state = octant->GetVisibilityState();

                        if (!Octree::IsVisible(&engine->GetWorld().GetOctree(), octant)) {
                            ++num_culled_objects;
                            continue;
                        }

                        if (!visibility_state.Get(scene_cull_id)) {
                            continue;
                        }
                    } else {
                        DebugLog(
                            LogType::Warn,
                            "In pipeline #%u: spatial #%u not in octree!\n",
                            m_id.value,
                            spatial->GetId().value
                        );

                        continue;
                    }
                }

                const auto spatial_index = spatial->GetId().value - 1;

                UInt material_index = 0;

                if (spatial->GetMaterial() != nullptr && spatial->GetMaterial()->IsReady()) {
                    // TODO: rather than checking each call we should just add once
                    material_index = spatial->GetMaterial()->GetId().value - 1;
                }

                const auto skeleton_index = spatial->GetSkeleton() != nullptr
                    ? spatial->GetSkeleton()->GetId().value - 1
                    : 0;

#if HYP_FEATURES_BINDLESS_TEXTURES
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index] },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(spatial_index  * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#else
                secondary->BindDescriptorSets(
                    instance->GetDescriptorPool(),
                    m_pipeline.get(),
                    FixedArray { DescriptorSet::object_buffer_mapping[frame_index], DescriptorSet::GetPerFrameIndex(DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES, material_index, frame_index) },
                    FixedArray { DescriptorSet::DESCRIPTOR_SET_INDEX_OBJECT,        DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES },
                    FixedArray {
                        UInt32(material_index * sizeof(MaterialShaderData)),
                        UInt32(spatial_index  * sizeof(ObjectShaderData)),
                        UInt32(skeleton_index * sizeof(SkeletonShaderData))
                    }
                );
#endif

                spatial->GetMesh()->Render(engine, secondary);
            }

            // DebugLog(
            //     LogType::Debug,
            //     "Scene %u: Culled %u objects\n",
            //     scene_cull_id.value,
            //     num_culled_objects
            // );

            HYPERION_RETURN_OK;
        });
    
    secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer());
}

} // namespace hyperion::v2
