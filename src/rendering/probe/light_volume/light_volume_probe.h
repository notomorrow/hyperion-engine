#ifndef ENVMAP_PROBE_H
#define ENVMAP_PROBE_H

#include "../probe.h"
#include "../../../math/matrix4.h"
#include "../../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {

class Shader;
class ComputeShader;
class Cubemap;

class LightVolumeProbe : public Probe {
public:
    LightVolumeProbe(const Vector3 &origin, const BoundingBox &bounds, const Vector3 &grid_offset);
    LightVolumeProbe(const LightVolumeProbe &other) = delete;
    LightVolumeProbe &operator=(const LightVolumeProbe &other) = delete;
    virtual ~LightVolumeProbe();

    virtual void Bind(Shader *shader) override;
    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

    Vector3 m_grid_offset;

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;
};
} // namespace hyperion

#endif
