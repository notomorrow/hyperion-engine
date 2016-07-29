#ifndef ANIMATION_H
#define ANIMATION_H

#include "animation_track.h"

#include <vector>
#include <string>

namespace apex {
class Animation {
public:
    Animation(const std::string &name);
    Animation(const Animation &other);

    const std::string &GetName() const;
    void SetName(const std::string &);

    float GetLength() const;

    void AddTrack(const AnimationTrack &);
    AnimationTrack &GetTrack(size_t index);
    const AnimationTrack &GetTrack(size_t index) const;
    size_t NumTracks() const;

    void Apply(float time);
    void ApplyBlended(float time, float blend);

private:
    std::string name;
    std::vector<AnimationTrack> tracks;
};
}

#endif