#ifndef HYPERION_V2_KEYFRAME_H
#define HYPERION_V2_KEYFRAME_H

#include <math/Transform.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

class Keyframe
{
public:
    Keyframe();
    Keyframe(const Keyframe &other) = default;
    Keyframe &operator=(const Keyframe &other) = default;
    Keyframe(Float time, const Transform &transform);

    Float GetTime() const
        { return m_time; }

    void SetTime(Float time)
        { m_time = time; }
   
    const Transform &GetTransform() const
        { return m_transform; }

    void SetTransform(const Transform &transform)
        { m_transform = transform; }

    Keyframe Blend(const Keyframe &to, Float blend) const;

private:
    Float m_time;
    Transform m_transform;
};

} // namespace hyperion::v2

#endif