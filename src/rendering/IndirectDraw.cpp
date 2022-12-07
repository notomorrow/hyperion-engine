#include "IndirectDraw.hpp"
#include <rendering/DrawCall.hpp>
#include <rendering/Mesh.hpp>

#include <math/MathUtil.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

using renderer::ShaderVec2;
using renderer::ShaderVec3;
using renderer::ShaderVec4;

class IndirectRenderer;

struct RENDER_COMMAND(CreateIndirectRenderer) : RenderCommand
{
    IndirectRenderer &indirect_renderer;

    RENDER_COMMAND(CreateIndirectRenderer)(IndirectRenderer &indirect_renderer)
        : indirect_renderer(indirect_renderer)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(indirect_renderer.m_indirect_draw_state.Create());

        const IndirectParams initial_params { };

        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            HYPERION_BUBBLE_ERRORS(indirect_renderer.m_indirect_params_buffers[frame_index].Create(
                Engine::Get()->GetGPUDevice(),
                sizeof(IndirectParams)
            ));

            indirect_renderer.m_indirect_params_buffers[frame_index].Copy(
                Engine::Get()->GetGPUDevice(),
                sizeof(IndirectParams),
                &initial_params
            );

            AssertThrow(indirect_renderer.m_descriptor_sets[frame_index] != nullptr);

            // global object data
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(0)
                ->SetElementBuffer(0, Engine::Get()->GetRenderData()->objects.GetBuffers()[frame_index].get());

            // global scene data
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::DynamicStorageBufferDescriptor>(1)
                ->SetSubDescriptor({
                    .buffer = Engine::Get()->GetRenderData()->scenes.GetBuffers()[frame_index].get(),
                    .range = static_cast<UInt>(sizeof(SceneShaderData))
                });

            // params buffer
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::UniformBufferDescriptor>(2)
                ->SetSubDescriptor({
                    .buffer = &indirect_renderer.m_indirect_params_buffers[frame_index]
                });

            // instances buffer
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(3)
                ->SetSubDescriptor({
                    .buffer = indirect_renderer.m_indirect_draw_state.GetInstanceBuffer(frame_index)
                });

            // indirect commands
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(4)
                ->SetSubDescriptor({
                    .buffer = indirect_renderer.m_indirect_draw_state.GetIndirectBuffer(frame_index)
                });

            // entity batches
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::StorageBufferDescriptor>(5)
                ->SetSubDescriptor({
                    .buffer = Engine::Get()->GetRenderData()->entity_instance_batches.GetBuffers()[frame_index].get(),
                });

            // depth pyramid image (set to placeholder)
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::ImageDescriptor>(6)
                ->SetSubDescriptor({
                    .image_view = &Engine::Get()->GetPlaceholderData().GetImageView2D1x1R8()
                });

            // sampler
            indirect_renderer.m_descriptor_sets[frame_index]->AddDescriptor<renderer::SamplerDescriptor>(7)
                ->SetSubDescriptor({
                    .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
                });

            HYPERION_BUBBLE_ERRORS(indirect_renderer.m_descriptor_sets[frame_index]->Create(
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyIndirectRenderer) : RenderCommand
{
    IndirectRenderer &indirect_renderer;

    RENDER_COMMAND(DestroyIndirectRenderer)(IndirectRenderer &indirect_renderer)
        : indirect_renderer(indirect_renderer)
    {
    }

    virtual Result operator()()
    {
        auto result = renderer::Result::OK;

        for (auto &params_buffer : indirect_renderer.m_indirect_params_buffers) {
            HYPERION_PASS_ERRORS(
                params_buffer.Destroy(Engine::Get()->GetGPUDevice()),
                result
            );
        }

        for (auto &descriptor_set : indirect_renderer.m_descriptor_sets) {
            HYPERION_PASS_ERRORS(
                descriptor_set->Destroy(Engine::Get()->GetGPUDevice()),
                result
            );
        }

        HYPERION_PASS_ERRORS(
            indirect_renderer.m_indirect_draw_state.Destroy(),
            result
        );

        return result;
    }
};

IndirectDrawState::IndirectDrawState()
    : m_num_draw_commands(0)
{
}

IndirectDrawState::~IndirectDrawState()
{
}

Result IndirectDrawState::Create()
{
    auto single_time_commands = Engine::Get()->GetGPUInstance()->GetSingleTimeCommands();

    single_time_commands.Push([this](CommandBuffer *command_buffer) -> Result {
        for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            auto frame = Frame::TemporaryFrame(command_buffer, frame_index);

            if (!ResizeIndirectDrawCommandsBuffer(&frame, initial_count)) {
                return { Result::RENDERER_ERR, "Failed to create indirect draw commands buffer!" };
            }

            if (!ResizeInstancesBuffer(&frame, initial_count)) {
                return { Result::RENDERER_ERR, "Failed to create instances buffer!" };
            }
        }

        HYPERION_RETURN_OK;
    });

    return single_time_commands.Execute(Engine::Get()->GetGPUDevice());
}

Result IndirectDrawState::Destroy()
{
    auto result = Result::OK;
    
    for (auto &buffer : m_indirect_buffers) {
        if (buffer == nullptr) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            buffer->Destroy(Engine::Get()->GetGPUDevice()),
            result
        );

        buffer.Reset();
    }
    
    for (auto &buffer : m_instance_buffers) {
        if (buffer == nullptr) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            buffer->Destroy(Engine::Get()->GetGPUDevice()),
            result
        );

        buffer.Reset();
    }

    for (auto &buffer : m_staging_buffers) {
        if (buffer == nullptr) {
            continue;
        }

        HYPERION_PASS_ERRORS(
            buffer->Destroy(Engine::Get()->GetGPUDevice()),
            result
        );

        buffer.Reset();
    }

    return result;
}

template <class BufferType>
static bool ResizeBuffer(
    Frame *frame,
    FixedArray<UniquePtr<BufferType>, max_frames_in_flight> &buffers,
    SizeType new_buffer_size
)
{
    const auto frame_index = frame->GetFrameIndex();

    SizeType current_buffer_size = 0u;

    bool needs_create = false;

    if (buffers[frame_index] != nullptr) {
        current_buffer_size = buffers[frame_index]->size;

        if (new_buffer_size > current_buffer_size) {
            HYPERION_ASSERT_RESULT(buffers[frame_index]->Destroy(Engine::Get()->GetGPUDevice()));

            needs_create = true;
        }
    } else {
        buffers[frame_index].Reset(new BufferType());

        needs_create = true;
    }

    if (needs_create) {
        if constexpr (IndirectDrawState::use_next_pow2_size) {
            new_buffer_size = MathUtil::NextPowerOf2(new_buffer_size);
        }

        DebugLog(
            LogType::Debug,
            "Resize indirect buffer (%p) at frame index %u from %llu -> %llu\n",
            buffers[frame_index].Get(),
            frame_index,
            current_buffer_size,
            new_buffer_size
        );

        HYPERION_ASSERT_RESULT(buffers[frame_index]->Create(Engine::Get()->GetGPUDevice(), new_buffer_size));

        return true;
    }

    return false;
}

void IndirectDrawState::PushDrawCall(const DrawCall &draw_call, DrawCommandData &out)
{
    out = { };

    if (draw_call.mesh == nullptr) {
        return;
    }

    const UInt32 draw_command_index = m_num_draw_commands++;
    
    ShaderVec4<UInt32> packed_data { 0, 0, 0, 0 };

    // first byte = bucket. we currently use only 7, with
    // some having the potential to be combined, so it shouldn't be
    // an issue.
    // packed_data[0] = (1u << UInt32(draw_call.p.bucket)) & 0xFF;

    // Memory::Copy()

    for (UInt index = 0; index < draw_call.entity_id_count; index++) {
        m_object_instances.PushBack(ObjectInstance {
            .entity_id = draw_call.entity_ids[index].Value(),
            .draw_command_index = draw_command_index,
            .instance_index = index,
            .batch_index = draw_call.batch_index
        });
    }

    out.draw_command_index = draw_command_index;

    if (m_draw_commands.Size() < SizeType(m_num_draw_commands)) {
        m_draw_commands.Resize(SizeType(m_num_draw_commands));
    }

    draw_call.mesh->PopulateIndirectDrawCommand(m_draw_commands[draw_command_index]);
    
    m_is_dirty = { true, true };
}

bool IndirectDrawState::ResizeIndirectDrawCommandsBuffer(Frame *frame, SizeType count)
{
    const bool was_created_or_resized = ResizeBuffer<IndirectBuffer>(
        frame,
        m_indirect_buffers,
        count * sizeof(IndirectDrawCommand)
    );

    if (!was_created_or_resized) {
        return false;
    }

    // upload zeros to the buffer using a staging buffer.
    if (m_staging_buffers[frame->GetFrameIndex()] != nullptr) {
        HYPERION_ASSERT_RESULT(m_staging_buffers[frame->GetFrameIndex()]->Destroy(Engine::Get()->GetGPUDevice()));
    } else {
        m_staging_buffers[frame->GetFrameIndex()].Reset(new StagingBuffer());
    }

    HYPERION_ASSERT_RESULT(m_staging_buffers[frame->GetFrameIndex()]->Create(
        Engine::Get()->GetGPUDevice(),
        m_indirect_buffers[frame->GetFrameIndex()]->size
    ));

    m_staging_buffers[frame->GetFrameIndex()]->Memset(
        Engine::Get()->GetGPUDevice(),
        m_staging_buffers[frame->GetFrameIndex()]->size,
        0x00 // fill buffer with zeros
    );

    m_indirect_buffers[frame->GetFrameIndex()]->CopyFrom(
        frame->GetCommandBuffer(),
        m_staging_buffers[frame->GetFrameIndex()].Get(),
        m_staging_buffers[frame->GetFrameIndex()]->size
    );

    return true;
}

bool IndirectDrawState::ResizeInstancesBuffer(Frame *frame, SizeType count)
{
    const bool was_created_or_resized = ResizeBuffer<StorageBuffer>(
        frame,
        m_instance_buffers,
        count * sizeof(ObjectInstance)
    );

    if (was_created_or_resized) {
        m_instance_buffers[frame->GetFrameIndex()]->Memset(
            Engine::Get()->GetGPUDevice(),
            m_instance_buffers[frame->GetFrameIndex()]->size,
            0x00
        );
    }

    return was_created_or_resized;
}

bool IndirectDrawState::ResizeIfNeeded(Frame *frame)
{
    // assume render thread

    bool resize_happened = false;
    
    if (m_is_dirty[frame->GetFrameIndex()] || m_indirect_buffers[frame->GetFrameIndex()] == nullptr) {
        resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, m_num_draw_commands);
    }
    
    if (m_is_dirty[frame->GetFrameIndex()] || m_instance_buffers[frame->GetFrameIndex()] == nullptr) {
        resize_happened |= ResizeInstancesBuffer(frame, m_object_instances.Size());
    }

    return resize_happened;
}

void IndirectDrawState::Reset()
{
    // assume render thread

    m_num_draw_commands = 0;

    m_object_instances.Clear();
    m_draw_commands.Clear();

    m_is_dirty = { true, true };
}

void IndirectDrawState::Reserve(Frame *frame, SizeType count)
{
    // assume render thread

    m_object_instances.Reserve(count);
    m_draw_commands.Reserve(count);

    bool resize_happened = false;
    
    resize_happened |= ResizeIndirectDrawCommandsBuffer(frame, count);
    resize_happened |= ResizeInstancesBuffer(frame, count);

    m_is_dirty[frame->GetFrameIndex()] |= resize_happened;
}

void IndirectDrawState::UpdateBufferData(Frame *frame, bool *out_was_resized)
{
    // assume render thread

    const auto frame_index = frame->GetFrameIndex();

    if ((*out_was_resized = ResizeIfNeeded(frame))) {
        m_is_dirty[frame_index] = true;
    }

    if (!m_is_dirty[frame_index]) {
        return;
    }
    
    // fill instances buffer with data of the meshes
    {
        AssertThrow(m_staging_buffers[frame_index] != nullptr);
        AssertThrow(m_staging_buffers[frame_index]->size >= sizeof(IndirectDrawCommand) * m_draw_commands.Size());

        // TODO: Use same setup as global buffers
        
        m_staging_buffers[frame_index]->Copy(
            Engine::Get()->GetGPUDevice(),
            m_draw_commands.Size() * sizeof(IndirectDrawCommand),
            m_draw_commands.Data()
        );

        m_indirect_buffers[frame_index]->CopyFrom(
            frame->GetCommandBuffer(),
            m_staging_buffers[frame_index].Get(),
            m_staging_buffers[frame_index]->size
        );
    }

    AssertThrow(m_instance_buffers[frame_index]->size >= m_object_instances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    m_instance_buffers[frame_index]->Copy(
        Engine::Get()->GetGPUDevice(),
        m_object_instances.Size() * sizeof(ObjectInstance),
        m_object_instances.Data()
    );
    
    m_is_dirty[frame_index] = false;
}



IndirectRenderer::IndirectRenderer()
    : m_cached_cull_data_updated_bits(0x0)
{
}

IndirectRenderer::~IndirectRenderer() = default;

void IndirectRenderer::Create()
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_descriptor_sets[frame_index] = UniquePtr<DescriptorSet>::Construct();
    }

    RenderCommands::Push<RENDER_COMMAND(CreateIndirectRenderer)>(*this);

    // create compute pipeline for object visibility (for indirect render)
    // TODO: cache pipelines: re-use this
    m_object_visibility = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("ObjectVisibility")),
        Array<const DescriptorSet *> { m_descriptor_sets[0].Get() }
    );

    InitObject(m_object_visibility);
}

void IndirectRenderer::Destroy()
{
    m_object_visibility.Reset();

    RenderCommands::Push<RENDER_COMMAND(DestroyIndirectRenderer)>(*this);

    HYP_SYNC_RENDER();
}

void IndirectRenderer::ExecuteCullShaderInBatches(Frame *frame, const CullData &cull_data)
{
    auto *command_buffer = frame->GetCommandBuffer();
    const UInt frame_index = frame->GetFrameIndex();

    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index) != nullptr);
    AssertThrow(m_indirect_draw_state.GetIndirectBuffer(frame_index)->size != 0);

    const UInt num_instances = UInt(m_indirect_draw_state.GetInstances().Size());
    const UInt num_batches = (num_instances / IndirectDrawState::batch_size) + 1;

    if (num_instances == 0) {
        return;
    }

    {
        bool was_buffer_resized = false;
        m_indirect_draw_state.UpdateBufferData(frame, &was_buffer_resized);

        if (was_buffer_resized) {
            RebuildDescriptors(frame);
        }
    }

    if (m_cached_cull_data != cull_data) {
        m_cached_cull_data = cull_data;
        m_cached_cull_data_updated_bits = 0xFF;
    }

    if (m_cached_cull_data_updated_bits & (1u << frame_index)) {
        m_descriptor_sets[frame_index]->GetDescriptor(6)->SetSubDescriptor({
            .element_index = 0,
            .image_view = m_cached_cull_data.depth_pyramid_image_views[frame_index]
        });

        m_descriptor_sets[frame_index]->ApplyUpdates(Engine::Get()->GetGPUDevice());

        m_cached_cull_data_updated_bits &= ~(1u << frame_index);
    }
    
    const ID<Scene> scene_id = Engine::Get()->GetRenderState().GetScene().id;
    const UInt scene_index = scene_id.ToIndex();

    // bind our descriptor set to binding point 0
    command_buffer->BindDescriptorSet(
        Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
        m_object_visibility->GetPipeline(),
        m_descriptor_sets[frame_index].Get(),
        static_cast<DescriptorSet::Index>(0),
        FixedArray { HYP_RENDER_OBJECT_OFFSET(Scene, scene_index) }
    );

    UInt count_remaining = num_instances;

    for (UInt batch_index = 0; batch_index < num_batches; batch_index++) {
        const UInt num_instances_in_batch = MathUtil::Min(count_remaining, IndirectDrawState::batch_size);

        m_object_visibility->GetPipeline()->Bind(command_buffer, Pipeline::PushConstantData {
            .object_visibility_data = {
                .batch_offset = batch_index * IndirectDrawState::batch_size,
                .num_instances = num_instances_in_batch,
                .scene_id = UInt32(scene_id.value),
                .depth_pyramid_dimensions = Extent2D(m_cached_cull_data.depth_pyramid_dimensions)
            }
        });
        
        m_object_visibility->GetPipeline()->Dispatch(command_buffer, Extent3D { 1, 1, 1 });

        count_remaining -= num_instances_in_batch;
    }
}

void IndirectRenderer::RebuildDescriptors(Frame *frame)
{
    const UInt frame_index = frame->GetFrameIndex();

    auto &descriptor_set = m_descriptor_sets[frame_index];

    descriptor_set->GetDescriptor(3)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(3)->SetElementBuffer(0, m_indirect_draw_state.GetInstanceBuffer(frame_index));

    descriptor_set->GetDescriptor(4)->RemoveSubDescriptor(0);
    descriptor_set->GetDescriptor(4)->SetElementBuffer(0, m_indirect_draw_state.GetIndirectBuffer(frame_index));

    descriptor_set->ApplyUpdates(Engine::Get()->GetGPUDevice());
}

} // namespace hyperion::v2