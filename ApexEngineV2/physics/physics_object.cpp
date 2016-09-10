#include "physics_object.h"

namespace apex {
PhysicsObject::PhysicsObject(const std::string &tag, 
    double mass, double restitution, PhysicsShape shape)
    : tag(tag), mass(mass), restitution(restitution), shape(shape), grounded(false), gravity(0, -5, 0)
{
}

PhysicsObject::~PhysicsObject()
{
}

const Vector3 &PhysicsObject::GetForce() const
{
    return force;
}

void PhysicsObject::SetForce(const Vector3 &vec)
{
    force = vec;
}

const Vector3 &PhysicsObject::GetAcceleration() const
{
    return acceleration;
}

void PhysicsObject::SetAcceleration(const Vector3 &vec)
{
    acceleration = vec;
}

const Vector3 &PhysicsObject::GetVelocity() const
{
    return velocity;
}

void PhysicsObject::SetVelocity(const Vector3 &vec)
{
    velocity = vec;
}

const Vector3 &PhysicsObject::GetGravity() const
{
    return gravity;
}

void PhysicsObject::SetGravity(const Vector3 &vec)
{
    gravity = vec;
}

void PhysicsObject::ApplyForce(const Vector3 &vec)
{
    force += vec;
}
} // namespace apex