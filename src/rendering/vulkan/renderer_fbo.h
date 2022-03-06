#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"
#include "renderer_attachment.h"

#include <memory>
#include <vector>

namespace hyperion {

class RendererFramebufferObject {
public:
    struct AttachmentInfo {
        RendererImage image;
        RendererImageView image_view;
        RendererSampler sampler;
    };

    RendererFramebufferObject(size_t width, size_t height);
    RendererFramebufferObject(const RendererFramebufferObject &other) = delete;
    RendererFramebufferObject &operator=(const RendererFramebufferObject &other) = delete;
    ~RendererFramebufferObject();

    RendererResult Create(RendererDevice *device);
    RendererResult Destroy(RendererDevice *device);

    void AddAttachment(std::unique_ptr<AttachmentInfo> &&attachment);

private:
    size_t m_width,
           m_height;

    std::vector<std::unique_ptr<AttachmentInfo>> m_attachment_infos;

    VkRenderPass m_render_pass;
    VkFramebuffer m_framebuffer;
};

} // namespace hyperion

#endif // RENDERER_FBO_H
