#ifndef ANIMATION_TRACK_H
#define ANIMATION_TRACK_H

#include "../math/vector3.h"
#include "../math/quaternion.h"
#include "bone.h"
#include "keyframe.h"

#include <vector>
#include <memory>

namespace hyperion {
class AnimationTrack {
public:
    AnimationTrack(std::shared_ptr<Bone> bone);

    void SetBone(std::shared_ptr<Bone>);
    std::shared_ptr<Bone> GetBone() const;

    void AddFrame(const Keyframe &frame);

    float GetLength() const;

    Keyframe GetPoseAt(float time) const;

private:
    std::vector<Keyframe> frames;
    std::shared_ptr<Bone> bone;
};
}

#endif
