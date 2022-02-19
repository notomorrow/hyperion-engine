#ifndef SPHERICAL_HARMONICS_CONTROL_H
#define SPHERICAL_HARMONICS_CONTROL_H

#include "../../../controls/entity_control.h"
#include "../../../math/bounding_box.h"
#include <memory>

namespace hyperion {
class SphericalHarmonicsProbe;
class SphericalHarmonicsControl : public EntityControl {
public:
    SphericalHarmonicsControl(const Vector3 &origin, BoundingBox bounds = BoundingBox());
    virtual ~SphericalHarmonicsControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<Control> CloneImpl() override;

    std::shared_ptr<Node> m_node;
    std::shared_ptr<SphericalHarmonicsProbe> m_probe;
    Vector3 m_origin;
    BoundingBox m_bounds;
};
} // namespace hyperion

#endif