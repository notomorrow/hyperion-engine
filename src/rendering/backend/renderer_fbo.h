#ifndef HYPERION_RENDERER_FBO_H
#define HYPERION_RENDERER_FBO_H

#include "renderer_attachment.h"
#include "renderer_image.h"
#include "renderer_image_view.h"
#include "renderer_sampler.h"

#include <memory>
#include <vector>

namespace hyperion {
namespace renderer {
class RenderPass;
class FramebufferObject {
public:
    struct AttachmentImageInfo {
        std::unique_ptr<Image> image;
        std::unique_ptr<ImageView> image_view;
        std::unique_ptr<Sampler> sampler;
        bool image_needs_creation; // is `image` newly constructed?
        bool image_view_needs_creation; // is `image_view` newly constructed?
        bool sampler_needs_creation; // is `sampler` newly constructed?
    };

    FramebufferObject(size_t width, size_t height);
    FramebufferObject(const FramebufferObject &other) = delete;
    FramebufferObject &operator=(const FramebufferObject &other) = delete;
    ~FramebufferObject();

    inline VkFramebuffer GetFramebuffer() const { return m_framebuffer; }

    Result AddAttachment(Texture::TextureInternalFormat format);
    Result AddAttachment(AttachmentImageInfo &&, Texture::TextureInternalFormat format);
    inline std::vector<AttachmentImageInfo> &GetAttachmentImageInfos() { return m_fbo_attachments; }
    inline const std::vector<AttachmentImageInfo> &GetAttachmentImageInfos() const
    {
        return m_fbo_attachments;
    }

    inline size_t GetWidth() const { return m_width; }
    inline size_t GetHeight() const { return m_height; }

    Result Create(Device *device, RenderPass *render_pass);
    Result Destroy(Device *device);

private:

    size_t m_width,
        m_height;

    std::vector<AttachmentImageInfo> m_fbo_attachments;

    VkFramebuffer m_framebuffer;
};

} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_H
