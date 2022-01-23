#ifndef BULLET_MATH_UTIL_H
#define BULLET_MATH_UTIL_H

#include "math/vector3.h"
#include "math/quaternion.h"

#include "LinearMath/btVector3.h"
#include "LinearMath/btQuaternion.h"

static inline btVector3 ToBulletVector(const hyperion::Vector3 &vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

static inline hyperion::Vector3 FromBulletVector(const btVector3 &vec)
{
    return hyperion::Vector3(vec.x(), vec.y(), vec.z());
}

static inline btQuaternion ToBulletQuaternion(const hyperion::Quaternion &quat)
{
    return btQuaternion(quat.x, quat.y, quat.z, quat.w);
}

static inline hyperion::Quaternion FromBulletQuaternion(const btQuaternion &quat)
{
    return hyperion::Quaternion(quat.x(), quat.y(), quat.z(), quat.w());
}


#endif
