#ifndef GI_MAPPER_CAMERA_H
#define GI_MAPPER_CAMERA_H

#include "../probe_region.h"
#include "../../../math/bounding_box.h"
#include "../../texture_3D.h"
#include "../../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

class GIMapperCamera : public Renderable {
public:
    GIMapperCamera(const ProbeRegion &region);
    ~GIMapperCamera();

    ProbeRegion &GetRegion() { return m_region; }
    const ProbeRegion &GetRegion() const { return m_region; }

    inline std::shared_ptr<Texture3D> &GetTexture() { return m_texture; }
    inline const std::shared_ptr<Texture3D> &GetTexture() const { return m_texture; }

    void Begin();
    void End();

    void Update(double dt);

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    unsigned int m_texture_id;
    std::shared_ptr<Texture3D> m_texture;
    ProbeRegion m_region;
    std::shared_ptr<ComputeShader> m_clear_shader;
    std::shared_ptr<ComputeShader> m_mipmap_shader;
    Camera *m_camera;
};
} // namespace hyperion

#endif