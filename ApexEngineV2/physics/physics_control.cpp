#include "physics_control.h"
#include "../entity.h"

namespace apex {
PhysicsControl::PhysicsControl(PhysicsObject *shape)
    : object(shape)
{
}

PhysicsControl::~PhysicsControl()
{
    if (object != nullptr) {
        delete object;
    }
}

void PhysicsControl::OnAdded()
{
    object->position = parent->GetLocalTranslation();
}

void PhysicsControl::OnRemoved()
{
}

void PhysicsControl::OnUpdate(double dt)
{
    const double timestep = 0.01;

    if (object->mass != 0.0) {
        if (!object->grounded) {
            object->force += object->gravity * object->mass;
        }
        object->acceleration = object->force / object->mass;
        object->velocity += object->acceleration * timestep * timestep * 0.5;

        object->velocity = Vector3::Clamp(object->velocity, -MAX_VELOCITY, MAX_VELOCITY);

        object->position += object->velocity * timestep;
        object->transform.SetTranslation(object->position);
        object->transform.SetScale(parent->GetLocalScale());
        parent->SetLocalTranslation(object->position);
        //parent->Move(object->velocity * timestep);
    } else {
        object->force = 0;
        object->acceleration = 0;
        object->velocity = 0;
        object->transform = parent->GetGlobalTransform();
    }
}

const PhysicsObject *PhysicsControl::GetPhysicsObject() const
{
    return object;
}
}