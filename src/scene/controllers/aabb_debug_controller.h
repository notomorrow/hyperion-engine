#ifndef HYPERION_V2_AABB_DEBUG_CONTROLLER_H
#define HYPERION_V2_AABB_DEBUG_CONTROLLER_H

#include "../controller.h"

#include <scene/spatial.h>
#include <math/bounding_box.h>

namespace hyperion::v2 {

class Engine;

class AabbDebugController : public Controller {
public:
    AabbDebugController(Engine *engine);
    virtual ~AabbDebugController() override = default;
    
    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    Engine      *m_engine;
    Ref<Spatial> m_aabb_entity;
    BoundingBox  m_aabb;
};

} // namespace hyperion::v2

#endif
