#ifndef HYPERION_V2_FILTER_STACK_H
#define HYPERION_V2_FILTER_STACK_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"

#include <rendering/mesh.h>

#include <memory>

namespace hyperion::renderer {
class Instance;
class Frame;
} // namespace hyperion::renderer

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Pipeline;

class FilterStack {
public:
    struct Filter {
        std::unique_ptr<Shader> shader; // TMP
        Pipeline *pipeline; // TMP
        Framebuffer::ID framebuffer;
        VkCommandBuffer cmd;
        VkSemaphore sp;
    };

    FilterStack();
    FilterStack(const FilterStack &) = delete;
    FilterStack &operator=(const FilterStack &) = delete;
    ~FilterStack();

    inline Mesh *GetFullScreenQuad() { return m_quad.get(); }
    inline const Mesh *GetFullScreenQuad() const { return m_quad.get(); }
    inline const std::array<Framebuffer::ID, 2> &GetFramebuffers() const { return m_framebuffers; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);
    std::vector<Filter> m_filters;

private:
    void RecordFilters(Engine *engine);

    std::shared_ptr<Mesh> m_quad; // TMP

    VkSemaphore wait_sp;
    VkFence m_fc_submit;
    VkCommandBuffer final_cmd_buffer;

    std::array<Framebuffer::ID, 2> m_framebuffers; /* ping, pong */
    std::vector<RenderPass::ID> m_render_passes; /* One per each filter */
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FILTER_STACK_H

