#include "Keyframe.hpp"

namespace hyperion::v2 {

Keyframe::Keyframe()
    : m_time(0.0f)
{
}

Keyframe::Keyframe(Float time, const Transform &transform)
    : m_time(time),
      m_transform(transform)
{
}

Keyframe Keyframe::Blend(const Keyframe &to, Float blend) const
{
    const Float time = MathUtil::Lerp(m_time, to.GetTime(), blend);

    Transform transform(m_transform);
    transform.GetTranslation().Lerp(to.GetTransform().GetTranslation(), blend);
    transform.GetScale().Lerp(to.GetTransform().GetScale(), blend);
    transform.GetRotation().Slerp(to.GetTransform().GetRotation(), blend);
    transform.UpdateMatrix();

    return { time, transform };
}


} // namespace hyperion::v2