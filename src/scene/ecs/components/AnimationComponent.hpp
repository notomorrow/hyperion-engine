/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <HashCode.hpp>

namespace hyperion {

enum class AnimationPlaybackStatus : uint32
{
    STOPPED = 0,
    PAUSED,
    PLAYING
};

enum class AnimationLoopMode : uint32
{
    ONCE = 0,
    REPEAT
};

HYP_STRUCT()

struct AnimationPlaybackState
{
    HYP_FIELD(Property = "AnimationIndex", Serialize = true, Editor = true)
    uint32 animationIndex = ~0u;

    HYP_FIELD(Property = "Status", Serialize = true, Editor = true)
    AnimationPlaybackStatus status = AnimationPlaybackStatus::STOPPED;

    HYP_FIELD(Property = "LoopMode", Serialize = true, Editor = true)
    AnimationLoopMode loopMode = AnimationLoopMode::ONCE;

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Serialize = true, Editor = true)
    float currentTime = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(animationIndex);
        hc.Add(status);
        hc.Add(loopMode);
        hc.Add(speed);
        hc.Add(currentTime);

        return hc;
    }
};

HYP_STRUCT(Component)

struct AnimationComponent
{
    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AnimationPlaybackState playbackState;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(playbackState);

        return hc;
    }
};

} // namespace hyperion
