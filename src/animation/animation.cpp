#include "animation.h"
#include "../math/math_util.h"

namespace hyperion {
Animation::Animation(const std::string &name)
    : name(name)
{
}

Animation::Animation(const Animation &other)
    : name(name),
      tracks(other.tracks)
{
}

const std::string &Animation::GetName() const
{
    return name;
}

void Animation::SetName(const std::string &str)
{
    name = str;
}

float Animation::GetLength() const
{
    if (tracks.empty()) {
        return 0.0;
    } else {
        return tracks.back().GetLength();
    }
}

void Animation::AddTrack(const AnimationTrack &track)
{
    tracks.push_back(track);
}

AnimationTrack &Animation::GetTrack(size_t index)
{
    return tracks.at(index);
}

const AnimationTrack &Animation::GetTrack(size_t index) const
{
    return tracks.at(index);
}

size_t Animation::NumTracks() const
{
    return tracks.size();
}

void Animation::Apply(float time)
{
    for (auto &&track : tracks) {
        track.GetBone()->ClearPose();
        track.GetBone()->ApplyPose(track.GetPoseAt(time));
    }
}

void Animation::ApplyBlended(float time, float blend)
{
    for (auto &&track : tracks) {
        if (blend <= 0.001f) {
            track.GetBone()->ClearPose();
        }

        auto frame = track.GetPoseAt(time);
        auto blended = track.GetBone()->GetCurrentPose().Blend(frame, 
            MathUtil::Clamp(blend, 0.0f, 1.0f));
        track.GetBone()->ApplyPose(blended);
    }
}
}
