#ifndef ENTITY_CONTROL_H
#define ENTITY_CONTROL_H

#include "control.h"

namespace hyperion {

class EntityControl : public Control {
    friend class Entity;
public:
    EntityControl(const fbom::FBOMType &loadable_type, const double tps = 30.0);
    EntityControl(const EntityControl &other) = delete;
    EntityControl &operator=(const EntityControl &other) = delete;
    virtual ~EntityControl() = default;

    virtual void OnUpdate(double dt) = 0;

protected:
    Entity *parent;
};

} // namespace hyperion

#endif
