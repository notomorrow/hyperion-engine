#ifndef DEFERRED_PIPELINE_H
#define DEFERRED_PIPELINE_H

#include "../framebuffer.h"
#include "../postprocess/filter_stack.h"

#include <memory>

namespace hyperion {
class Renderer;
class Camera;
class Framebuffer2D;
class Mesh;
class PostFilter;
class DeferredPipeline {
public:
    DeferredPipeline();
    DeferredPipeline(const DeferredPipeline &other) = delete;
    DeferredPipeline(DeferredPipeline &&other) = delete;
    DeferredPipeline &operator=(const DeferredPipeline &other) = delete;
    ~DeferredPipeline();

    FilterStack *GetPreFilterStack() { return m_pre_filters; }
    const FilterStack *GetPreFilterStack() const { return m_pre_filters; }

    FilterStack *GetPostFilterStack() { return m_post_filters; }
    const FilterStack *GetPostFilterStack() const { return m_post_filters; }

    void Render(Renderer *, Camera *, Framebuffer2D *);

private:
    void RenderOpaqueBuckets(Renderer *, Camera *, Framebuffer2D *);
    void InitializeBlitFbo(Framebuffer2D *read_fbo);
    void InitializeGBuffer(Framebuffer2D *read_fbo);
    void CopyFboTextures(Framebuffer2D *read_fbo);

    Framebuffer::FramebufferAttachments_t m_gbuffer;
    bool m_gbuffer_initialized;
    FilterStack *m_pre_filters;
    FilterStack *m_post_filters;

    std::shared_ptr<Mesh> m_quad;
    PostFilter *m_deferred_filter;

    Framebuffer2D *m_blit_fbo;
};

} // namespace hyperion

#endif