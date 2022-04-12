#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "base.h"
#include <rendering/camera/camera.h>

namespace hyperion::v2 {

class Scene : public EngineComponentBase<STUB_CLASS(Scene)> {
public:
    Scene(std::unique_ptr<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    void SetCamera(std::unique_ptr<Camera> &&camera) { m_camera = std::move(camera); }
    Camera *GetCamera() const { return m_camera.get(); }
    
    void Init(Engine *engine);
    void Update(Engine *engine, double delta_time);

private:
    void UpdateShaderData(Engine *engine) const;

    std::unique_ptr<Camera> m_camera;
};

} // namespace hyperion::v2

#endif