#ifndef GI_MAPPER_H
#define GI_MAPPER_H

#include "../../math/bounding_box.h"
#include "../renderable.h"

#include <memory>

namespace hyperion {
class Shader;
class ComputeShader;

struct GIMapperRegion {
    BoundingBox bounds;
};

class GIMapper : public Renderable {
public:
    GIMapper(const GIMapperRegion &region);
    ~GIMapper();

    GIMapperRegion &GetRegion() { return m_region; }
    const GIMapperRegion &GetRegion() const { return m_region; }

    inline unsigned int GetTextureId() const { return m_texture_id; }

    void Bind(Shader *shader);

    void Begin();
    void End();

    virtual void Render(Renderer *renderer, Camera *cam) override;
    void UpdateRenderTick(double dt);

private:
    unsigned int m_texture_id;
    GIMapperRegion m_region;
    std::shared_ptr<ComputeShader> m_clear_shader;
    double m_render_tick;
};
} // namespace apex

#endif