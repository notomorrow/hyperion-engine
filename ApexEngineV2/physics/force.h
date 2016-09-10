#ifndef FORCE_H
#define FORCE_H

#include "rigid_body.h"

namespace apex {
class Force {
public:
    virtual ~Force() = default;

    virtual void UpdateOnBody(RigidBody *body, double dt) = 0;
};
} // namespace apex

#endif