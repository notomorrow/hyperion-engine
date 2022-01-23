#ifndef KEYFRAME_H
#define KEYFRAME_H

#include "../math/vector3.h"
#include "../math/quaternion.h"

namespace hyperion {
class Keyframe {
public:
    Keyframe();
    Keyframe(const Keyframe &other);
    Keyframe(float time, const Vector3 &trans, const Quaternion &rot);

    float GetTime() const;
    void SetTime(float);
    const Vector3 &GetTranslation() const;
    void SetTranslation(const Vector3 &);
    const Quaternion &GetRotation() const;
    void SetRotation(const Quaternion &);

    Keyframe Blend(const Keyframe &to, float blend) const;

private:
    float time;
    Vector3 translation;
    Quaternion rotation;
};
}

#endif
