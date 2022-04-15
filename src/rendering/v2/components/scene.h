#ifndef HYPERION_V2_SCENE_H
#define HYPERION_V2_SCENE_H

#include "base.h"
#include "texture.h"
#include "shader.h"
#include <rendering/camera/camera.h>

namespace hyperion::v2 {

class Scene : public EngineComponentBase<STUB_CLASS(Scene)> {
public:
    static constexpr uint32_t max_environment_textures = SceneShaderData::max_environment_textures;

    Scene(std::unique_ptr<Camera> &&camera);
    Scene(const Scene &other) = delete;
    Scene &operator=(const Scene &other) = delete;
    ~Scene();

    Camera *GetCamera() const { return m_camera.get(); }
    void SetCamera(std::unique_ptr<Camera> &&camera) { m_camera = std::move(camera); }

    Texture *GetEnvironmentTexture(uint32_t index) const
        { return m_environment_textures[index].ptr; }

    void SetEnvironmentTexture(uint32_t index, Ref<Texture> &&texture);
    
    void Init(Engine *engine);
    void Update(Engine *engine, double delta_time);

private:
    void UpdateShaderData(Engine *engine) const;

    std::unique_ptr<Camera> m_camera;
    std::array<Ref<Texture>, max_environment_textures> m_environment_textures;
    mutable ShaderDataState m_shader_data_state;
};

} // namespace hyperion::v2

#endif