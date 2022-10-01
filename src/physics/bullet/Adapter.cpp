#include <physics/bullet/Adapter.hpp>
#include <physics/PhysicsWorld.hpp>
#include <core/lib/UniquePtr.hpp>
#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>

#include "btBulletDynamicsCommon.h"

namespace hyperion::v2::physics {

static inline btVector3 ToBtVector(const Vector3 &vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

static inline Vector3 FromBtVector(const btVector3 &vec)
{
    return Vector3(vec.x(), vec.y(), vec.z());
}

static inline btQuaternion ToBtQuaternion(const Quaternion &quat)
{
    return btQuaternion(quat.x, quat.y, quat.z, quat.w);
}

static inline Quaternion FromBtQuaternion(const btQuaternion &quat)
{
    return Quaternion(quat.x(), quat.y(), quat.z(), quat.w());
}

struct RigidBodyInternalData
{
    UniquePtr<btRigidBody> rigid_body;
    UniquePtr<btMotionState> motion_state;
};

static UniquePtr<btCollisionShape> CreatePhysicsShapeHandle(PhysicsShape *physics_shape)
{
    switch (physics_shape->GetType()) {
    case PhysicsShapeType::BOX:
        return UniquePtr<btBoxShape>::Construct(ToBtVector(static_cast<BoxPhysicsShape *>(physics_shape)->GetAABB().GetExtent()));
    case PhysicsShapeType::SPHERE:
        return UniquePtr<btSphereShape>::Construct(static_cast<SpherePhysicsShape *>(physics_shape)->GetSphere().GetRadius());
    // others...
    default:
        AssertThrowMsg(false, "Unknown PhysicsShapeType!");
    }
}

void BulletPhysicsAdapter::Init(PhysicsWorldBase *world)
{
    m_collision_configuration = new btDefaultCollisionConfiguration();
    m_dispatcher = new btCollisionDispatcher(m_collision_configuration);
    m_broadphase = new btDbvtBroadphase();
    m_solver = new btSequentialImpulseConstraintSolver();
    m_dynamics_world = new btDiscreteDynamicsWorld(
        m_dispatcher,
        m_broadphase,
        m_solver,
        m_collision_configuration
    );

    m_dynamics_world->setGravity(btVector3(
        world->GetGravity().x,
        world->GetGravity().y,
        world->GetGravity().z
    ));
}

void BulletPhysicsAdapter::Teardown(PhysicsWorldBase *world)
{
    delete m_dynamics_world;
    delete m_solver;
    delete m_broadphase;
    delete m_dispatcher;
    delete m_collision_configuration;
}

void BulletPhysicsAdapter::Tick(PhysicsWorldBase *world, GameCounter::TickUnitHighPrec delta)
{
    m_dynamics_world->stepSimulation(delta);

    for (auto &rigid_body : world->GetRigidBodies()) {
        std::cout << "Update rigidbody " << rigid_body->GetID().value << "\n";

        auto *internal_data = static_cast<RigidBodyInternalData *>(rigid_body->GetHandle().Get());

        btTransform bt_transform;
        internal_data->motion_state->getWorldTransform(bt_transform);

        auto &rigid_body_transform = rigid_body->GetTransform();
        rigid_body_transform.GetTranslation() = FromBtVector(bt_transform.getOrigin());
        rigid_body_transform.GetRotation() = FromBtQuaternion(bt_transform.getRotation());
        rigid_body_transform.UpdateMatrix();
    }
}

void BulletPhysicsAdapter::OnRigidBodyAdded(Handle<RigidBody> &rigid_body)
{
    AssertThrow(rigid_body != nullptr);
    AssertThrowMsg(rigid_body->GetShape() != nullptr,
        "No PhysicsShape on RigidBody!");

    if (!rigid_body->GetShape()->GetHandle()) {
        rigid_body->GetShape()->SetHandle(CreatePhysicsShapeHandle(rigid_body->GetShape().Get()));
    }

    btVector3 local_inertia(0, 0, 0);

    if (rigid_body->IsKinematic() && rigid_body->GetPhysicsMaterial().GetMass() != 0.0f) {
        static_cast<btCollisionShape *>(rigid_body->GetShape()->GetHandle().Get())
            ->calculateLocalInertia(rigid_body->GetPhysicsMaterial().GetMass(), local_inertia);
    }

    UniquePtr<RigidBodyInternalData> internal_data;
    internal_data.Reset(new RigidBodyInternalData);

    btTransform bt_transform;
    bt_transform.setIdentity();
    bt_transform.setOrigin(ToBtVector(rigid_body->GetTransform().GetTranslation()));
    bt_transform.setRotation(ToBtQuaternion(rigid_body->GetTransform().GetRotation()));
    internal_data->motion_state.Reset(new btDefaultMotionState(bt_transform));

    btRigidBody::btRigidBodyConstructionInfo construction_info(
        rigid_body->GetPhysicsMaterial().GetMass(),
        internal_data->motion_state.Get(),
        static_cast<btCollisionShape *>(rigid_body->GetShape()->GetHandle().Get()),
        local_inertia
    );

    internal_data->rigid_body.Reset(new btRigidBody(construction_info));

    m_dynamics_world->addRigidBody(internal_data->rigid_body.Get());

    rigid_body->SetHandle(std::move(internal_data));
}

void BulletPhysicsAdapter::OnRigidBodyRemoved(Handle<RigidBody> &rigid_body)
{
    
}

} // namespace hyperion::v2::physics