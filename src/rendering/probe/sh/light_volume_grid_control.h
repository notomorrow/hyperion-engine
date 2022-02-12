#ifndef LIGHT_VOLUME_GRID_CONTROL_H
#define LIGHT_VOLUME_GRID_CONTROL_H

#include "../../../controls/entity_control.h"
#include "../../../math/bounding_box.h"
#include <memory>

namespace hyperion {
class LightVolumeGrid;
class LightVolumeGridControl : public EntityControl {
public:
    LightVolumeGridControl(const Vector3 &origin, BoundingBox bounds = BoundingBox());
    virtual ~LightVolumeGridControl() = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(double dt);

protected:
    virtual std::shared_ptr<Control> CloneImpl() override;

    std::shared_ptr<Entity> m_light_volume_grid_node;
    std::shared_ptr<LightVolumeGrid> m_light_volume_grid;
};
} // namespace hyperion

#endif