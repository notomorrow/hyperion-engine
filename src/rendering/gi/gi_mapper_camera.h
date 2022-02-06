#ifndef GI_MAPPER_CAMERA_H
#define GI_MAPPER_CAMERA_H

#include "../../math/bounding_box.h"
#include "../texture_3D.h"
#include "../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

struct GIMapperRegion {
    BoundingBox bounds;
    Vector3 direction;
    Vector3 up_vector;
    int direction_index;
};

class GIMapperCamera : public Renderable {
public:
    GIMapperCamera(const GIMapperRegion &region);
    ~GIMapperCamera();

    GIMapperRegion &GetRegion() { return m_region; }
    const GIMapperRegion &GetRegion() const { return m_region; }

    inline std::shared_ptr<Texture3D> &GetTexture() { return m_texture; }
    inline const std::shared_ptr<Texture3D> &GetTexture() const { return m_texture; }

    void Begin();
    void End();

    void Update(double dt);

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    unsigned int m_texture_id;
    std::shared_ptr<Texture3D> m_texture;
    GIMapperRegion m_region;
    std::shared_ptr<ComputeShader> m_clear_shader;
    std::shared_ptr<ComputeShader> m_mipmap_shader;
    Camera *m_camera;
};
} // namespace apex

#endif