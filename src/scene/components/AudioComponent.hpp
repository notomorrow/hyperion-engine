/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <audio/AudioSource.hpp>
#include <core/Handle.hpp>

#include <HashCode.hpp>
#include <GameCounter.hpp>

namespace hyperion {

using AudioComponentFlags = uint32;

enum AudioComponentFlagBits : AudioComponentFlags
{
    AUDIO_COMPONENT_FLAG_NONE = 0x0,
    AUDIO_COMPONENT_FLAG_INIT = 0x1,
};

enum AudioPlaybackStatus
{
    AUDIO_PLAYBACK_STATUS_STOPPED,
    AUDIO_PLAYBACK_STATUS_PAUSED,
    AUDIO_PLAYBACK_STATUS_PLAYING
};

enum AudioLoopMode
{
    AUDIO_LOOP_MODE_ONCE,
    AUDIO_LOOP_MODE_REPEAT
};

HYP_STRUCT()
struct AudioPlaybackState
{
    HYP_FIELD(Property = "Status", Serialize = true, Editor = true)
    AudioPlaybackStatus status = AUDIO_PLAYBACK_STATUS_STOPPED;

    HYP_FIELD(Property = "LoopMode", Serialize = true, Editor = true)
    AudioLoopMode loopMode = AUDIO_LOOP_MODE_ONCE;

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "CurrentTime", Serialize = true, Editor = true)
    float currentTime = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(status);
        hashCode.Add(loopMode);
        hashCode.Add(speed);
        hashCode.Add(currentTime);

        return hashCode;
    }
};

HYP_STRUCT(Component, Label = "Audio Component", Description = "Controls the state of an audio source.", Editor = true)
struct AudioComponent
{
    HYP_FIELD(Property = "AudioSource", Serialize = true, Editor = true)
    Handle<AudioSource> audioSource;

    HYP_FIELD(Property = "PlaybackState", Serialize = true, Editor = true)
    AudioPlaybackState playbackState;

    HYP_FIELD()
    AudioComponentFlags flags = AUDIO_COMPONENT_FLAG_NONE;

    HYP_FIELD()
    Vec3f lastPosition;

    HYP_FIELD()
    float timer = 0.0f;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(audioSource);
        hashCode.Add(playbackState);

        return hashCode;
    }
};

} // namespace hyperion
