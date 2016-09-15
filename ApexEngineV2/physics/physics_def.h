#ifndef PHYSICS_DEF_H
#define PHYSICS_DEF_H

#define EXPERIMENTAL_PHYSICS 1
#if EXPERIMENTAL_PHYSICS
#define RIGID_BODY apex::physics::Rigidbody
#else
#define RIGID_BODY apex::RigidBody
#endif

#endif