#ifndef HYPERION_V2_FILTER_STACK_H
#define HYPERION_V2_FILTER_STACK_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"

#include <rendering/mesh.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <memory>

namespace hyperion::renderer {
class Instance;
class Frame;
} // namespace hyperion::renderer

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Pipeline;
using renderer::CommandBuffer;

class FilterStack {
public:
    struct Filter {
        std::unique_ptr<Shader> shader; // TMP
        Pipeline *pipeline; // TMP
        Framebuffer::ID framebuffer;
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
    void Render(Engine *engine, Frame *frame, uint32_t frame_index);

    std::vector<Filter> m_filters;
    std::vector<std::unique_ptr<CommandBuffer>> m_filter_command_buffers;
    std::vector<VkSemaphore> m_filter_semaphores;

private:
    void RecordFilters(Engine *engine);

    std::shared_ptr<Mesh> m_quad; // TMP


    VkSemaphore wait_sp;
    VkCommandBuffer final_cmd_buffer;

    std::vector<VkFence> m_frame_fences;
    std::array<Framebuffer::ID, 2> m_framebuffers; /* ping, pong */
    std::vector<RenderPass::ID> m_render_passes; /* One per each filter */
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_FILTER_STACK_H

