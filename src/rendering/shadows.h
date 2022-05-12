#ifndef HYPERION_V2_SHADOWS_H
#define HYPERION_V2_SHADOWS_H

#include "post_fx.h"
#include "renderer.h"

#include <scene/scene.h>
#include <camera/camera.h>

namespace hyperion::v2 {

class ShadowEffect : public PostEffect {
public:
    ShadowEffect();
    ShadowEffect(const ShadowEffect &other) = delete;
    ShadowEffect &operator=(const ShadowEffect &other) = delete;
    ~ShadowEffect();

    Scene *GetScene() const { return m_scene.ptr; }

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreatePipeline(Engine *engine);
    void Create(Engine *engine, std::unique_ptr<Camera> &&camera);

    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:
    Ref<Scene> m_scene;
};

class ShadowRenderer : public Renderer {
public:
    ShadowRenderer(std::unique_ptr<Camera> &&camera);
    ShadowRenderer(const ShadowRenderer &other) = delete;
    ShadowRenderer &operator=(const ShadowRenderer &other) = delete;
    ~ShadowRenderer();

    ShadowEffect &GetEffect() { return m_effect; }
    const ShadowEffect &GetEffect() const { return m_effect; }

    Scene *GetScene() const { return m_effect.GetScene(); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, CommandBuffer *primary, uint32_t frame_index);

private:
    ShadowEffect m_effect;
    std::unique_ptr<Camera> m_camera;
};


} // namespace hyperion::v2

#endif