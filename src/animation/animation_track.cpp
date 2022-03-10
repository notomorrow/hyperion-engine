#include "animation_track.h"

namespace hyperion {
AnimationTrack::AnimationTrack(std::shared_ptr<Bone> bone) :
    bone(bone)
{
}

void AnimationTrack::SetBone(std::shared_ptr<Bone> b)
{
    bone = b;
}

std::shared_ptr<Bone> AnimationTrack::GetBone() const
{
    return bone;
}

void AnimationTrack::AddFrame(const Keyframe &frame)
{
    frames.push_back(frame);
}

float AnimationTrack::GetLength() const
{
    if (frames.empty()) {
        return 0.0f;
    } else {
        return frames.back().GetTime();
    }
}

Keyframe AnimationTrack::GetPoseAt(float time) const
{
    int first = 0, second = -1;

    if (frames.empty()) {
        return Keyframe(time, Vector3(), Quaternion());
    }

    for (int i = 0; i < int(frames.size() - 1); i++) {
        if (MathUtil::InRange(time, { frames[i].GetTime(), frames[i + 1].GetTime() })) {
            first = i;
            second = i + 1;
            break;
        }
    }

    const Keyframe &current = frames.at(first);

    Vector3 tmp_trans = current.GetTranslation();
    Quaternion tmp_rot = current.GetRotation();

    if (second > first) {
        const Keyframe &next = frames[second];

        float fraction = (time - current.GetTime()) / 
            (next.GetTime() - current.GetTime());

        tmp_trans.Lerp(next.GetTranslation(), fraction);
        tmp_rot.Slerp(next.GetRotation(), fraction);
    }

    return Keyframe(time, tmp_trans, tmp_rot);
}
}
