#ifndef HYPERION_V2_INDIRECT_DRAW_H
#define HYPERION_V2_INDIRECT_DRAW_H

#include <core/lib/Queue.hpp>

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::IndirectDrawCommand;

class Mesh;
class Material;

struct DrawCall {
    Mesh *mesh         = nullptr;
    Material *material = nullptr;

    IDBase scene_id,
           entity_id,
           material_id,
           skeleton_id;
};

struct IndirectDrawState {
    std::unique_ptr<renderer::IndirectBuffer> buffer;
    Queue<DrawCall>                           commands;

    void PushDrawCall(DrawCall &&draw_call)
    {
        commands.Push(std::move(draw_call));
    }
};

} // namespace hyperion::v2

#endif