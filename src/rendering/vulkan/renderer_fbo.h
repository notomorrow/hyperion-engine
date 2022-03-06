#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "renderer_render_pass.h"
#include "renderer_attachment.h"

#include <memory>
#include <vector>

namespace hyperion {

class RendererFramebufferObject {
public:

    RendererFramebufferObject(size_t width, size_t height);
    RendererFramebufferObject(const RendererFramebufferObject &other) = delete;
    RendererFramebufferObject &operator=(const RendererFramebufferObject &other) = delete;
    ~RendererFramebufferObject();

    inline RendererRenderPass *GetRenderPass() { return m_render_pass.get(); }
    inline const RendererRenderPass *GetRenderPass() const { return m_render_pass.get(); }

    RendererResult Create(RendererDevice *device);
    RendererResult Destroy(RendererDevice *device);

private:
    struct AttachmentImageInfo {
        std::unique_ptr<RendererImage> image;
        std::unique_ptr<RendererImageView> image_view;
        std::unique_ptr<RendererSampler> sampler;
    };

    size_t m_width,
           m_height;

    std::unique_ptr<RendererRenderPass> m_render_pass;
    std::vector<AttachmentImageInfo> m_fbo_attachments;

    VkFramebuffer m_framebuffer;
};

} // namespace hyperion

#endif // RENDERER_FBO_H
