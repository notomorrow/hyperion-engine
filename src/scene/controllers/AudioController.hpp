#ifndef HYPERION_V2_AUDIO_CONTROLLER_H
#define HYPERION_V2_AUDIO_CONTROLLER_H

#include "PlaybackController.hpp"

#include <audio/AudioSource.hpp>
#include <math/Vector3.hpp>

#include <memory>

namespace hyperion::v2 {

class AudioController : public PlaybackController
{
public:
    AudioController();
    AudioController(Handle<AudioSource> &&source);
    virtual ~AudioController() override = default;

    Handle<AudioSource> &GetSource() { return m_source; }
    const Handle<AudioSource> &GetSource() const { return m_source; }
    void SetSource(Handle<AudioSource> &&source);

    virtual void Play(float speed, LoopMode loop_mode = LoopMode::ONCE) override;
    virtual void Stop() override;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    Handle<AudioSource> m_source;
    Vector3 m_last_position;
    GameCounter::TickUnit m_timer;
};

} // namespace hyperion::v2

#endif
