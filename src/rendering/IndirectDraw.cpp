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

void IndirectDrawState::PushDrawable(Drawable &&drawable)
{
    if (drawable.mesh == nullptr) {
        return;
    }

    drawable.object_instance = ObjectInstance {
        .entity_id          = static_cast<UInt32>(drawable.entity_id.value),
        .draw_command_index = static_cast<UInt32>(m_drawables.Size()),
        .batch_index        = static_cast<UInt32>(m_object_instances.Size() / 256u),
        .num_indices        = static_cast<UInt32>(drawable.mesh->NumIndices()),
        .aabb_max           = drawable.bounding_box.max.ToVector4(),
        .aabb_min           = drawable.bounding_box.min.ToVector4(),
        .bounding_sphere    = drawable.bounding_sphere.ToVector4()
    };

    m_object_instances.PushBack(std::move(drawable.object_instance));
    m_drawables.PushBack(std::move(drawable));

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
        m_drawables.Size() * sizeof(IndirectDrawCommand)
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
        m_drawables.Size() * sizeof(ObjectInstance)
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

    m_drawables.Clear();
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

} // namespace hyperion::v2