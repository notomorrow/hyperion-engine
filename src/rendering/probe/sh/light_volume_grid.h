#ifndef LIGHT_VOLUME_GRID_H
#define LIGHT_VOLUME_GRID_H

#include "../probe.h"
#include "light_volume_probe.h"
#include "../../framebuffer_2d.h"
#include "../../../math/matrix4.h"
#include "../../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {

class Shader;
class ComputeShader;
class Texture3D;
class Cubemap;

class LightVolumeGrid : public Probe {
public:
    static const int cubemap_width;

    LightVolumeGrid(const Vector3 &origin, const BoundingBox &bounds, int num_probes);
    LightVolumeGrid(const LightVolumeGrid &other) = delete;
    LightVolumeGrid &operator=(const LightVolumeGrid &other) = delete;
    virtual ~LightVolumeGrid();

    virtual void Bind(Shader *shader) override;
    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

    std::vector<LightVolumeProbe *> m_light_volumes;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    int m_num_probes;

    std::shared_ptr<Texture3D> m_grid_volume_texture; // 3d texture of low-res cubemaps
    // format:
    //   cubemap size 2*2*(6 sides)
    //   * numprobes*numprobes*numprobes
    //   
    double m_render_tick;
    int m_render_index;
    bool m_is_first_run;
};
} // namespace hyperion

#endif
