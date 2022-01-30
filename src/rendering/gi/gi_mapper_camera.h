#ifndef GI_MAPPER_CAMERA_H
#define GI_MAPPER_CAMERA_H

#include "../../math/bounding_box.h"
#include "../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

struct GIMapperRegion {
    BoundingBox bounds;
    Vector3 direction;
    Vector3 up_vector;
};

class GIMapperCamera : public Renderable {
public:
    GIMapperCamera(const GIMapperRegion &region);
    ~GIMapperCamera();

    GIMapperRegion &GetRegion() { return m_region; }
    const GIMapperRegion &GetRegion() const { return m_region; }

    inline unsigned int GetTextureId() const { return m_texture_id; }

    void Begin();
    void End();

    void Update(double dt);

    virtual void Render(Renderer *renderer, Camera *cam) override;

private:
    unsigned int m_texture_id;
    GIMapperRegion m_region;
    std::shared_ptr<ComputeShader> m_clear_shader;
    Camera *m_camera;
};
} // namespace apex

#endif