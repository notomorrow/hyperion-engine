#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "renderer_attachment.h"
#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"

#include <memory>
#include <vector>

namespace hyperion {
class RendererRenderPass;
class RendererFramebufferObject {
public:
    struct AttachmentImageInfo {
        std::unique_ptr<RendererImage> image;
        std::unique_ptr<RendererImageView> image_view;
        std::unique_ptr<RendererSampler> sampler;
        bool image_needs_creation; // is `image` newly constructed?
        bool image_view_needs_creation; // is `image_view` newly constructed?
        bool sampler_needs_creation; // is `sampler` newly constructed?
    };

    RendererFramebufferObject(size_t width, size_t height);
    RendererFramebufferObject(const RendererFramebufferObject &other) = delete;
    RendererFramebufferObject &operator=(const RendererFramebufferObject &other) = delete;
    ~RendererFramebufferObject();

    inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }

    RendererResult AddAttachment(Texture::TextureInternalFormat format, bool is_depth_attachment);
    RendererResult AddAttachment(AttachmentImageInfo &&, bool is_depth_attachment);
    inline std::vector<AttachmentImageInfo> &GetAttachmentImageInfos() { return m_fbo_attachments; }
    inline const std::vector<AttachmentImageInfo> &GetAttachmentImageInfos() const
        { return m_fbo_attachments; }

    inline size_t GetWidth() const { return m_width; }
    inline size_t GetHeight() const { return m_height; }

    RendererResult Create(RendererDevice *device, RendererRenderPass *render_pass);
    RendererResult Destroy(RendererDevice *device);

private:

    size_t m_width,
           m_height;

    std::vector<AttachmentImageInfo> m_fbo_attachments;

    VkFramebuffer m_framebuffer;
};

} // namespace hyperion

#endif // RENDERER_FBO_H
