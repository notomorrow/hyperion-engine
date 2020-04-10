#ifndef APEX_PHYSICS_PHYSICS_MATERIAL_H
#define APEX_PHYSICS_PHYSICS_MATERIAL_H

#include "../math/vector3.h"
#include "../math/matrix4.h"

#include <float.h>

namespace apex {
namespace physics {
class PhysicsMaterial {
public:
    PhysicsMaterial(double mass = 1.0, double friction = 0.8, double restitution = 0.2,
        double linear_damping = 0.85, double angular_damping = 0.7);
    PhysicsMaterial(const PhysicsMaterial &other);

    inline double GetMass() const { return m_mass == 0.0 ? DBL_MAX : m_mass; }
    inline void SetMass(double mass) { m_mass = mass; }
    inline double GetInverseMass() const { return m_mass == 0.0 ? 0.0 : 1.0 / m_mass; }
    inline double GetFriction() const { return m_friction; }
    inline void SetFriction(double friction) { m_friction = friction; }
    inline double GetRestitution() const { return m_restitution; }
    inline void SetRestitution(double restitution) { m_restitution = restitution; }
    inline double GetLinearDamping() const { return m_linear_damping; }
    inline void SetLinearDamping(double linear_damping) { m_linear_damping = linear_damping; }
    inline double GetAngularDamping() const { return m_angular_damping; }
    inline void SetAngularDamping(double angular_damping) { m_angular_damping = angular_damping; }

private:
    double m_mass;
    double m_friction;
    double m_restitution;
    double m_linear_damping;
    double m_angular_damping;
};
} // namespace physics
} // namespace apex

#endif
