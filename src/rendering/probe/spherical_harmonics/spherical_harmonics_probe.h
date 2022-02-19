#ifndef SPHERICAL_HARMONICS_PROBE_H
#define SPHERICAL_HARMONICS_PROBE_H

#include "../probe.h"
#include "../../framebuffer_cube.h"
#include "../../../math/matrix4.h"
#include "../../../math/vector3.h"

#include <memory>
#include <array>

namespace hyperion {

class Shader;
class ComputeShader;
class Cubemap;

class SphericalHarmonicsProbe : public Probe {
public:
    SphericalHarmonicsProbe(const Vector3 &origin, const BoundingBox &bounds);
    SphericalHarmonicsProbe(const SphericalHarmonicsProbe &other) = delete;
    SphericalHarmonicsProbe &operator=(const SphericalHarmonicsProbe &other) = delete;
    virtual ~SphericalHarmonicsProbe();

    virtual void Bind(Shader *shader) override;
    virtual void Update(double dt) override;
    virtual void Render(Renderer *renderer, Camera *cam) override;

    std::shared_ptr<Texture2D> m_sh_texture; // tmp

private:
    virtual std::shared_ptr<Renderable> CloneImpl() override;

    bool m_needs_rerender;
    std::shared_ptr<Cubemap> m_cubemap;
};
} // namespace hyperion

#endif
