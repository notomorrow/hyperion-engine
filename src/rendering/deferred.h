#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "post_fx.h"
#include "renderer.h"

#include <rendering/backend/renderer_frame.h>

namespace hyperion::v2 {

using renderer::Frame;

class DeferredRenderingEffect : public FullScreenPass {
public:
    DeferredRenderingEffect();
    DeferredRenderingEffect(const DeferredRenderingEffect &other) = delete;
    DeferredRenderingEffect &operator=(const DeferredRenderingEffect &other) = delete;
    ~DeferredRenderingEffect();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    inline DeferredRenderingEffect &GetEffect() { return m_effect; }
    inline const DeferredRenderingEffect &GetEffect() const { return m_effect; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredRenderingEffect m_effect;
    PostProcessing          m_post_processing;
};

} // namespace hyperion::v2

#endif