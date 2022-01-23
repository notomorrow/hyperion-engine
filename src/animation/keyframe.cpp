#include "keyframe.h"
#include "../math/math_util.h"

namespace hyperion {
Keyframe::Keyframe()
    : time(0.0f)
{
}

Keyframe::Keyframe(const Keyframe &other)
    : time(other.time), translation(other.translation), rotation(other.rotation)
{
}

Keyframe::Keyframe(float time, const Vector3 &translation, const Quaternion &rotation)
    : time(time), translation(translation), rotation(rotation)
{
}

float Keyframe::GetTime() const
{
    return time;
}

void Keyframe::SetTime(float f)
{
    time = f;
}

const Vector3 &Keyframe::GetTranslation() const
{
    return translation;
}

void Keyframe::SetTranslation(const Vector3 &vec)
{
    translation = vec;
}

const Quaternion &Keyframe::GetRotation() const
{
    return rotation;
}

void Keyframe::SetRotation(const Quaternion &quat)
{
    rotation = quat;
}

Keyframe Keyframe::Blend(const Keyframe &to, float blend) const
{
    float tmp_time = MathUtil::Lerp(time, to.GetTime(), blend);

    Vector3 tmp_trans = translation;
    tmp_trans.Lerp(to.GetTranslation(), blend);

    Quaternion tmp_rot = rotation;
    tmp_rot.Slerp(to.GetRotation(), blend);

    return Keyframe(tmp_time, tmp_trans, tmp_rot);
}
}
