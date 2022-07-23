#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include "Base.hpp"
#include "DrawProxy.hpp"

#include <core/lib/Queue.hpp>
#include <core/lib/FixedArray.hpp>
#include <core/lib/DynArray.hpp>

#include <math/BoundingSphere.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;
using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::StagingBuffer;
using renderer::Frame;
using renderer::Result;

class Mesh;
class Material;
class Engine;
class Spatial;

class IndirectDrawState {
    static constexpr SizeType initial_count = 1 << 8;

public:
    IndirectDrawState();
    ~IndirectDrawState();

    StorageBuffer *GetInstanceBuffer(UInt frame_index) const
        { return m_instance_buffers[frame_index].get(); }

    IndirectBuffer *GetIndirectBuffer(UInt frame_index) const
        { return m_indirect_buffers[frame_index].get(); }

    DynArray<EntityDrawProxy> &GetDrawables()             { return m_drawables; }
    const DynArray<EntityDrawProxy> &GetDrawables() const { return m_drawables; }

    Result Create(Engine *engine);
    Result Destroy(Engine *engine);

    void PushDrawable(EntityDrawProxy &&drawable);
    void ResetDrawables();

    void UpdateBufferData(Engine *engine, Frame *frame, bool *out_was_resized);

private:
    bool ResizeIndirectDrawCommandsBuffer(Engine *engine, Frame *frame);
    bool ResizeInstancesBuffer(Engine *engine, Frame *frame);

    // returns true if resize happened.
    bool ResizeIfNeeded(Engine *engine, Frame *frame);

    DynArray<ObjectInstance>      m_object_instances;
    DynArray<EntityDrawProxy>      m_drawables;

    FixedArray<std::unique_ptr<IndirectBuffer>, max_frames_in_flight> m_indirect_buffers;
    FixedArray<std::unique_ptr<StorageBuffer>, max_frames_in_flight>  m_instance_buffers;
    bool m_is_dirty = false;

};

} // namespace hyperion::v2

#endif