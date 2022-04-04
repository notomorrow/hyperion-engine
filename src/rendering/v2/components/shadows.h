#ifndef HYPERION_V2_SHADOWS_H
#define HYPERION_V2_SHADOWS_H

#include "post_fx.h"
#include "renderer.h"

namespace hyperion::v2 {

class ShadowEffect : public PostEffect {
public:
    ShadowEffect();
    ShadowEffect(const ShadowEffect &other) = delete;
    ShadowEffect &operator=(const ShadowEffect &other) = delete;
    ~ShadowEffect();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreatePipeline(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);
};

class ShadowRenderer : public Renderer {
public:
    ShadowRenderer();
    ShadowRenderer(const ShadowRenderer &other) = delete;
    ShadowRenderer &operator=(const ShadowRenderer &other) = delete;
    ~ShadowRenderer();

    inline ShadowEffect &GetEffect() { return m_effect; }
    inline const ShadowEffect &GetEffect() const { return m_effect; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:
    ShadowEffect m_effect;
};


} // namespace hyperion::v2

#endif