#ifndef PROBE_RENDERER_H
#define PROBE_RENDERER_H

#include "probe.h"
#include "../renderer.h"
#include "../camera/camera.h"

#include <array>
#include <memory>

namespace hyperion {

class Shader;
class Framebuffer;
class Cubemap;

class ProbeRenderer {
public:
    ProbeRenderer(int width = 256, int height = 256);
    ~ProbeRenderer();

    inline std::shared_ptr<Cubemap> GetColorTexture() const
    {
        return std::static_pointer_cast<Cubemap>(m_fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_COLOR));
    }

    inline std::shared_ptr<Cubemap> GetDepthTexture() const
    {
        return std::static_pointer_cast<Cubemap>(m_fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_DEPTH));
    }

    inline const Probe *GetProbe() const { return m_probe; }
    inline Probe *GetProbe() { return m_probe; }

    inline bool RenderShading() const { return m_render_shading; }
    void SetRenderShading(bool value);
    inline bool RenderTextures() const { return m_render_textures; }
    void SetRenderTextures(bool value);

    void Render(Renderer *renderer, Camera *camera);

private:
    std::shared_ptr<Shader> m_cubemap_renderer_shader;
    Framebuffer *m_fbo;
    Probe *m_probe;
    bool m_render_shading;
    bool m_render_textures;

    void UpdateUniforms();
};

} // namespace hyperion

#endif
