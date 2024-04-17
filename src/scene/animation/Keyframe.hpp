/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_KEYFRAME_HPP
#define HYPERION_KEYFRAME_HPP

#include <math/Transform.hpp>
#include <Types.hpp>

namespace hyperion {

class HYP_API Keyframe
{
public:
    Keyframe();
    Keyframe(const Keyframe &other) = default;
    Keyframe &operator=(const Keyframe &other) = default;
    Keyframe(float time, const Transform &transform);

    float GetTime() const
        { return m_time; }

    void SetTime(float time)
        { m_time = time; }
   
    const Transform &GetTransform() const
        { return m_transform; }

    void SetTransform(const Transform &transform)
        { m_transform = transform; }

    Keyframe Blend(const Keyframe &to, float blend) const;

private:
    float       m_time;
    Transform   m_transform;
};

} // namespace hyperion

#endif