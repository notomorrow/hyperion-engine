#ifndef HYPERION_V2_FRAMEBUFFER_H
#define HYPERION_V2_FRAMEBUFFER_H

#include "base.h"

#include "render_pass.h"
#include <rendering/backend/renderer_fbo.h>

#include <memory>

namespace hyperion::v2 {

using renderer::SpirvObject;

class Framebuffer : public BaseComponent<renderer::FramebufferObject> {
public:
    explicit Framebuffer(size_t width, size_t height);
    Framebuffer(const Framebuffer &) = delete;
    Framebuffer &operator=(const Framebuffer &) = delete;
    ~Framebuffer();

    inline RenderPass *GetRenderPass() { return m_render_pass.get(); }
    inline const RenderPass *GetRenderPass() const { return m_render_pass.get(); }

    void Create(Engine *engine, RenderPass *render_pass);
    void Destroy(Engine *engine);

private:
    non_owning_ptr<RenderPass> m_render_pass;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FRAMEBUFFER_H

