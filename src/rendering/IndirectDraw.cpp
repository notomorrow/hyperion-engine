#include "IndirectDraw.hpp"
#include "Mesh.hpp"

#include <math/MathUtil.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

IndirectDrawState::IndirectDrawState()
{
    for (auto &buffer : m_indirect_buffers) {
        buffer.reset(new IndirectBuffer());
    }

    for (auto &buffer : m_instance_buffers) {
        buffer.reset(new StorageBuffer());
    }
}

IndirectDrawState::~IndirectDrawState()
{
}

Result IndirectDrawState::Create(Engine *engine)
{
    auto result = renderer::Result::OK;
    
    for (auto &buffer : m_indirect_buffers) {
        HYPERION_PASS_ERRORS(
            buffer->Create(engine->GetDevice(), initial_count * sizeof(IndirectDrawCommand)),
            result
        );
    }
    
    for (auto &buffer : m_instance_buffers) {
        HYPERION_PASS_ERRORS(
            buffer->Create(engine->GetDevice(), initial_count * sizeof(ObjectInstance)),
            result
        );
    }

    return result;
}

Result IndirectDrawState::Destroy(Engine *engine)
{
    auto result = renderer::Result::OK;
    
    for (auto &buffer : m_indirect_buffers) {
        if (buffer == nullptr) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            buffer->Destroy(engine->GetDevice()),
            result
        );
    }
    
    for (auto &buffer : m_instance_buffers) {
        if (buffer == nullptr) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            buffer->Destroy(engine->GetDevice()),
            result
        );
    }

    return result;
}

template <class BufferType>
static bool ResizeBuffer(
    Engine *engine,
    Frame *frame,
    FixedArray<std::unique_ptr<BufferType>, max_frames_in_flight> &buffers,
    SizeType new_buffer_size
)
{
    const auto frame_index = frame->GetFrameIndex();

    SizeType current_buffer_size = 0u;

    bool needs_create = false;

    if (buffers[frame_index] != nullptr) {
        current_buffer_size = buffers[frame_index]->size;

        if (new_buffer_size > current_buffer_size) {
            HYPERION_ASSERT_RESULT(buffers[frame_index]->Destroy(engine->GetDevice()));

            needs_create = true;
        }
    } else {
        buffers[frame_index].reset(new BufferType());

        needs_create = true;
    }

    if (needs_create) {
        const SizeType new_buffer_size_pow2 = MathUtil::NextPowerOf2(new_buffer_size);

        DebugLog(
            LogType::Debug,
            "Resize indirect draw commands at frame index %u from %llu -> %llu\n",
            frame_index,
            current_buffer_size,
            new_buffer_size_pow2
        );

        HYPERION_ASSERT_RESULT(buffers[frame_index]->Create(engine->GetDevice(), new_buffer_size_pow2));

        return true;
    }

    return false;
}

void IndirectDrawState::PushDrawable(EntityDrawProxy &&draw_proxy)
{
    if (draw_proxy.mesh == nullptr) {
        return;
    }

    draw_proxy.object_instance = ObjectInstance {
        .entity_id          = static_cast<UInt32>(draw_proxy.entity_id.value),
        .draw_command_index = static_cast<UInt32>(m_draw_proxys.Size()),
        .batch_index        = static_cast<UInt32>(m_object_instances.Size() / batch_size),
        .num_indices        = static_cast<UInt32>(draw_proxy.mesh->NumIndices()),
        .aabb_max           = draw_proxy.bounding_box.max.ToVector4(),
        .aabb_min           = draw_proxy.bounding_box.min.ToVector4()
    };

    m_object_instances.PushBack(std::move(draw_proxy.object_instance));
    m_draw_proxys.PushBack(std::move(draw_proxy));

    m_is_dirty = true;
}

bool IndirectDrawState::ResizeIndirectDrawCommandsBuffer(Engine *engine, Frame *frame)
{
    const bool needs_update = m_is_dirty || m_indirect_buffers[frame->GetFrameIndex()] == nullptr;

    if (!needs_update) {
        return false;
    }

    const bool was_created_or_resized = ResizeBuffer<IndirectBuffer>(
        engine,
        frame,
        m_indirect_buffers,
        m_draw_proxys.Size() * sizeof(IndirectDrawCommand)
    );

    if (!was_created_or_resized) {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    // auto staging_buffer = std::make_unique<StagingBuffer>();

    // HYPERION_ASSERT_RESULT(staging_buffer->Create(
    //     engine->GetDevice(),
    //     m_indirect_buffers[frame->GetFrameIndex()]->size
    // ));

    // staging_buffer->Memset(
    //     engine->GetDevice(),
    //     staging_buffer->size,
    //     0 // fill buffer with zeros
    // );

    // m_indirect_buffers[frame->GetFrameIndex()]->CopyFrom(
    //     frame->GetCommandBuffer(),
    //     staging_buffer.get(),
    //     staging_buffer->size
    // );

    // engine->render_scheduler.Enqueue([engine, creation_frame = frame->GetFrameIndex(), buffer = staging_buffer.release()]() {
    //     return buffer->Destroy(engine->GetDevice());
    // });

    return true;
}

bool IndirectDrawState::ResizeInstancesBuffer(Engine *engine, Frame *frame)
{
    const bool needs_update = m_is_dirty || m_instance_buffers[frame->GetFrameIndex()] == nullptr;

    if (!needs_update) {
        return false;
    }

    return ResizeBuffer<StorageBuffer>(
        engine,
        frame,
        m_instance_buffers,
        m_draw_proxys.Size() * sizeof(ObjectInstance)
    );
}

bool IndirectDrawState::ResizeIfNeeded(Engine *engine, Frame *frame)
{
    // assume render thread

    bool resize_happened = false;

    resize_happened |= ResizeIndirectDrawCommandsBuffer(engine, frame);
    resize_happened |= ResizeInstancesBuffer(engine, frame);

    return resize_happened;
}

void IndirectDrawState::ResetDrawables()
{
    // assume render thread

    m_draw_proxys.Clear();
    m_object_instances.Clear();
}

void IndirectDrawState::UpdateBufferData(Engine *engine, Frame *frame, bool *out_was_resized)
{
    // assume render thread

    if ((*out_was_resized = ResizeIfNeeded(engine, frame))) {
        m_is_dirty = true;
    }

    if (!m_is_dirty) {
        return;
    }

    const auto frame_index = frame->GetFrameIndex();

    // update data for object instances (cpu - gpu)
    m_instance_buffers[frame_index]->Copy(
        engine->GetDevice(),
        m_object_instances.Size() * sizeof(ObjectInstance),
        m_object_instances.Data()
    );

    m_is_dirty = false;
}



IndirectRenderer::IndirectRenderer()
    : m_cached_cull_data_updated({ false })
{
}

IndirectRenderer::~IndirectRenderer()
{
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
                .buffer = engine->shader_globals->objects.GetBuffers()[frame_index].get(),
                .range  = static_cast<UInt>(sizeof(ObjectShaderData))
            });

        // global scene data
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
            ->SetSubDescriptor({
                .buffer = engine->shader_globals->scenes.GetBuffers()[frame_index].get(),
                .range  = static_cast<UInt>(sizeof(SceneShaderData))
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

        // depth pyramid image (set to placeholder)
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(5)
            ->SetSubDescriptor({
                .image_view = &engine->GetPlaceholderData().GetImageView2D1x1R8()
            });

        // sampler
        m_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(6)
            ->SetSubDescriptor({
                .sampler = &engine->GetPlaceholderData().GetSamplerNearest()
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

void IndirectRenderer::ExecuteCullShaderInBatches(
    Engine *engine,
    Frame *frame,
    const CullData &cull_data
)
{
    auto *command_buffer     = frame->GetCommandBuffer();
    const auto frame_index   = frame->GetFrameIndex();

    const UInt num_draw_proxys = static_cast<UInt>(m_indirect_draw_state.GetDrawables().Size());
    const UInt num_batches   = (num_draw_proxys / IndirectDrawState::batch_size) + 1;

    if (num_draw_proxys == 0) {
        return;
    }

    bool was_buffer_resized = false;
    m_indirect_draw_state.UpdateBufferData(engine, frame, &was_buffer_resized);

    if (was_buffer_resized) {
        RebuildDescriptors(engine, frame);
    }

    if (m_cached_cull_data != cull_data) {
        m_cached_cull_data = cull_data;
        m_cached_cull_data_updated = { true, true };
    }

    if (m_cached_cull_data_updated[frame_index]) {
        m_descriptor_sets[frame_index]->GetDescriptor(5)->SetSubDescriptor({
            .element_index = 0,
            .image_view    = m_cached_cull_data.depth_pyramid_image_views[frame_index]
        });

        m_descriptor_sets[frame_index]->ApplyUpdates(engine->GetDevice());

        m_cached_cull_data_updated[frame_index] = false;
    }

    const auto scene_id    = engine->render_state.GetScene().id;
    const UInt scene_index = scene_id ? scene_id.value - 1 : 0;

    // bind our descriptor set to binding point 0
    command_buffer->BindDescriptorSet(
        engine->GetInstance()->GetDescriptorPool(),
        m_object_visibility->GetPipeline(),
        m_descriptor_sets[frame_index].get(),
        static_cast<DescriptorSet::Index>(0),
        FixedArray { static_cast<UInt32>(scene_index * sizeof(SceneShaderData)) }
    );

    UInt count_remaining = num_draw_proxys;

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        const UInt num_draw_proxys_in_batch = MathUtil::Min(count_remaining, IndirectDrawState::batch_size);

        m_object_visibility->GetPipeline()->Bind(command_buffer, Pipeline::PushConstantData {
            .object_visibility_data = {
                .batch_offset             = batch_index * IndirectDrawState::batch_size,
                .num_draw_proxys            = num_draw_proxys_in_batch,
                .scene_id                 = static_cast<UInt32>(scene_id.value),
                .depth_pyramid_dimensions = Extent2D(m_cached_cull_data.depth_pyramid_dimensions)
            }
        });

        m_object_visibility->GetPipeline()->Dispatch(command_buffer, Extent3D { 1, 1, 1 });

        count_remaining -= num_draw_proxys_in_batch;
    }
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

} // namespace hyperion::v2