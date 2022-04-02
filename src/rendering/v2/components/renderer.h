#ifndef HYPERION_V2_RENDERER_H
#define HYPERION_V2_RENDERER_H

#include "render_list.h"

namespace hyperion::v2 {

class Renderer {
public:
    Renderer();
    Renderer(const Renderer &other) = delete;
    Renderer &operator=(const Renderer &other) = delete;
    ~Renderer();

    inline RenderList &GetRenderList() { return m_render_list; }
    inline const RenderList &GetRenderList() const { return m_render_list; }

protected:
    void RenderOpaqueObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index);
    void RenderTransparentObjects(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

    RenderList m_render_list;
};

} // namespace hyperion::v2

#endif