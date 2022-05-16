#ifndef HYPERION_V2_ENVIRONMENT_H
#define HYPERION_V2_ENVIRONMENT_H

#include "base.h"
#include "shadows.h"
#include "light.h"

#include <mutex>
#include <vector>

namespace hyperion::v2 {

class Engine;

class Environment : public EngineComponentBase<STUB_CLASS(Environment)> {
    using ShadowRendererPtr = std::unique_ptr<ShadowRenderer>;

public:
    Environment();
    Environment(const Environment &other) = delete;
    Environment &operator=(const Environment &other) = delete;
    ~Environment();
    
    Ref<Light> &GetLight(size_t index)                    { return m_lights[index]; }
    const Ref<Light> &GetLight(size_t index) const        { return m_lights[index]; }
    void AddLight(Ref<Light> &&light);
    size_t NumLights() const                              { return m_lights.size(); }
    const std::vector<Ref<Light>> &GetLights() const      { return m_lights; }

    size_t NumShadowRenderers() const                     { return m_shadow_renderers.size(); }
    ShadowRenderer *GetShadowRenderer(size_t index) const { return m_shadow_renderers[index].get(); }

    void AddShadowRenderer(Engine *engine, std::unique_ptr<ShadowRenderer> &&shadow_renderer);
    void RemoveShadowRenderer(Engine *engine, size_t index);

    void Init(Engine *engine);
    void RenderShadows(Engine *engine);

private:
    std::vector<Ref<Light>>                      m_lights;
    std::vector<ShadowRendererPtr>               m_shadow_renderers;
    bool                                         m_ready;
};

} // namespace hyperion::v2

#endif