#ifndef SHADOW_MAP_CONTROL_H
#define SHADOW_MAP_CONTROL_H

#include "../../controls/entity_control.h"
#include "../../math/bounding_box.h"
#include <memory>

namespace hyperion {
class ShadowMapping;
class ShadowMapControl : public EntityControl {
public:
    ShadowMapControl(const Vector3 &direction, double max_dist);
    virtual ~ShadowMapControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<Control> CloneImpl() override;

    std::shared_ptr<Node> m_node;
    std::shared_ptr<ShadowMapping> m_shadow_map_renderer;
    Vector3 m_direction;
    double m_max_dist;
};
} // namespace hyperion

#endif