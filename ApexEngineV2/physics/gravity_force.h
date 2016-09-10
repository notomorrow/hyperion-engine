#ifndef GRAVITY_FORCE_H
#define GRAVITY_FORCE_H

#include "force.h"
#include "../math/vector3.h"

namespace apex {
class GravityForce : public Force {
public:
    static const Vector3 GRAVITY;

    virtual void UpdateOnBody(RigidBody *body, double dt);
};
} // namespace apex

#endif