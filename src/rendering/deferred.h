#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "full_screen_pass.h"
#include "post_fx.h"
#include "renderer.h"
#include "texture.h"

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;

class DeferredPass : public FullScreenPass {
public:
    DeferredPass();
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    auto &GetMipmappedResults() { return m_mipmapped_results; }
    auto &GetSampler()          { return m_sampler; }

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    std::array<Ref<Texture>, max_frames_in_flight>        m_mipmapped_results;
    std::unique_ptr<Sampler>                              m_sampler;
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    DeferredPass &GetPass()                          { return m_pass; }
    const DeferredPass &GetPass() const              { return m_pass; }

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredPass   m_pass;
    PostProcessing m_post_processing;
};

} // namespace hyperion::v2

#endif