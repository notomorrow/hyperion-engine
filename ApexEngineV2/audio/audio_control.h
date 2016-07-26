#ifndef AUDIO_CONTROL_H
#define AUDIO_CONTROL_H

#include "../control.h"
#include "audio_source.h"

#include <memory>

namespace apex {
class AudioControl : public EntityControl {
public:
    AudioControl(std::shared_ptr<AudioSource> source);

    std::shared_ptr<AudioSource> GetSource();
    void SetSource(std::shared_ptr<AudioSource> src);

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    std::shared_ptr<AudioSource> source;
    Vector3 last_position;
};
}

#endif