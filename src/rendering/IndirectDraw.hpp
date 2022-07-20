#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include "Base.hpp"

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

struct alignas(16) ObjectInstance {
    UInt32  entity_id;
    UInt32  draw_command_index;
    UInt32  batch_index;
    UInt32  num_indices;
    Vector4 aabb_max;
    Vector4 aabb_min;
    Vector4 bounding_sphere;
};

struct Drawable {
    // rendering objects (sent from game thread to
    // rendering thread when updates are enqueued)
    // engine will hold onto RenderResources like Mesh and Material
    // for at least {max_frames_in_flight} frames, so we will have
    // rendered the Drawable before the ptr is invalidated.
    Mesh           *mesh     = nullptr;
    Material       *material = nullptr;

    IDBase         entity_id,
                   scene_id,
                   mesh_id,
                   material_id,
                   skeleton_id;

    BoundingBox    bounding_box;
    BoundingSphere bounding_sphere;

    // object instance in GPU
    ObjectInstance object_instance;
};

class IndirectDrawState {
    static constexpr SizeType initial_count = 1 << 8;

public:
    IndirectDrawState();
    ~IndirectDrawState();

    StorageBuffer *GetInstanceBuffer(UInt frame_index) const
        { return m_instance_buffers[frame_index].get(); }

    IndirectBuffer *GetIndirectBuffer(UInt frame_index) const
        { return m_indirect_buffers[frame_index].get(); }

    DynArray<Drawable> &GetDrawables()             { return m_drawables; }
    const DynArray<Drawable> &GetDrawables() const { return m_drawables; }

    Result Create(Engine *engine);
    Result Destroy(Engine *engine);

    void PushDrawable(Drawable &&drawable);
    void ResetDrawables();

    void UpdateBufferData(Engine *engine, Frame *frame, bool *out_was_resized);

private:
    bool ResizeIndirectDrawCommandsBuffer(Engine *engine, Frame *frame);
    bool ResizeInstancesBuffer(Engine *engine, Frame *frame);

    // returns true if resize happened.
    bool ResizeIfNeeded(Engine *engine, Frame *frame);

    DynArray<ObjectInstance>      m_object_instances;
    DynArray<Drawable>            m_drawables;

    FixedArray<std::unique_ptr<IndirectBuffer>, max_frames_in_flight> m_indirect_buffers;
    FixedArray<std::unique_ptr<StorageBuffer>, max_frames_in_flight>  m_instance_buffers;
    bool m_is_dirty = false;

};

} // namespace hyperion::v2

#endif