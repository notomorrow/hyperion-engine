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

    const Keyframe *current = nullptr;
    const Keyframe *next = nullptr;

    if (!frames.empty()) {
        size_t n = frames.size() - 1;
        for (size_t i = 0; i < n; i++) {
            if (time >= frames[i].GetTime() && 
                time <= frames[i + 1].GetTime()) {
                first = i;
                second = i + 1;
                break;
            }
        }

        current = &frames.at(first);

        Vector3 tmp_trans = current->GetTranslation();
        Quaternion tmp_rot = current->GetRotation();

        if (second > first) {
            next = &frames[second];

            float fraction = (time - current->GetTime()) / 
                (next->GetTime() - current->GetTime());

            tmp_trans.Lerp(next->GetTranslation(), fraction);
            tmp_rot.Slerp(next->GetRotation(), fraction);
        }

        return Keyframe(time, tmp_trans, tmp_rot);
    }
}
}
