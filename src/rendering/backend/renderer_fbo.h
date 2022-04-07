#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "renderer_attachment.h"
#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_render_pass.h"

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {

class FramebufferObject {
public:
    FramebufferObject(uint32_t width, uint32_t height);
    FramebufferObject(const FramebufferObject &other) = delete;
    FramebufferObject &operator=(const FramebufferObject &other) = delete;
    ~FramebufferObject();

    inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }

    void AddRenderPassAttachmentRef(AttachmentRef *attachment_ref)
    {
        attachment_ref->IncRef();

        m_render_pass_attachment_refs.push_back(attachment_ref);
    }

    inline auto &GetRenderPassAttachmentRefs() { return m_render_pass_attachment_refs; }
    inline const auto &GetRenderPassAttachmentRefs() const { return m_render_pass_attachment_refs; }

    inline uint32_t GetWidth() const { return m_width; }
    inline uint32_t GetHeight() const { return m_height; }

    Result Create(Device *device, RenderPass *render_pass);
    Result Destroy(Device *device);

private:

    uint32_t m_width,
             m_height;

    std::vector<AttachmentRef *> m_render_pass_attachment_refs;

    VkFramebuffer m_framebuffer;
};

} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
