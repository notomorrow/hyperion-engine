#ifndef POST_FILTER_H
#define POST_FILTER_H

#include <string>
#include <memory>

#include "../shaders/post_shader.h"
#include "../camera/camera.h"
#include "../framebuffer_2d.h"
#include "../material.h"

namespace apex {
class PostFilter {
public:
    PostFilter(const std::shared_ptr<PostShader> &shader);
    virtual ~PostFilter() = default;

    virtual void SetUniforms(Camera *cam) = 0;

    inline std::shared_ptr<PostShader> &GetShader() { return m_shader; }
    inline const std::shared_ptr<PostShader> &GetShader() const { return m_shader; }

    void Begin(Camera *cam, const Framebuffer::FramebufferAttachments_t &attachments);
    void End(Camera *cam, Framebuffer *fbo, Framebuffer::FramebufferAttachments_t &attachments);

protected:
    std::shared_ptr<PostShader> m_shader;
    Material m_material;
};
}; // namespace apex

#endif
