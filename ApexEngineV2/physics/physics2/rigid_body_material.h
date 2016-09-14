#ifndef APEX_PHYSICS_RIGID_BODY_MATERIAL_H
#define APEX_PHYSICS_RIGID_BODY_MATERIAL_H

#include "../../math/vector3.h"
#include "../../math/matrix4.h"

namespace apex {
struct RigidBodyMaterial {
    double mass;
    Vector3 position;
    Matrix4 transform;
};
} // namespace apex

#endif