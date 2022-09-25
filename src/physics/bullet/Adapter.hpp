#ifndef HYPERION_V2_PHYSICS_BULLET_ADAPTER_HPP
#define HYPERION_V2_PHYSICS_BULLET_ADAPTER_HPP

#include <physics/Adapter.hpp>

struct btDbvtBroadphase;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btDiscreteDynamicsWorld;

namespace hyperion::v2::physics {

class BulletPhysicsAdapter : public PhysicsAdapter<BulletPhysicsAdapter>
{
public:
    void Init(PhysicsWorldBase *world);
    void Teardown(PhysicsWorldBase *world);
    void Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta);

    void OnRigidBodyAdded(Handle<RigidBody> &rigid_body);
    void OnRigidBodyRemoved(const Handle<RigidBody> &rigid_body);

private:
    btDbvtBroadphase *m_broadphase;
    btDefaultCollisionConfiguration *m_collision_configuration;
    btCollisionDispatcher *m_dispatcher;
    btSequentialImpulseConstraintSolver *m_solver;
    btDiscreteDynamicsWorld *m_dynamics_world;
};

} // namespace hyperion::v2::physics

#endif