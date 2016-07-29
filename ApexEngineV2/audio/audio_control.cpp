#include "audio_control.h"
#include "../entity.h"

namespace apex {
AudioControl::AudioControl(std::shared_ptr<AudioSource> source)
    : EntityControl(10), source(source)
{
}

std::shared_ptr<AudioSource> AudioControl::GetSource()
{
    return source;
}

void AudioControl::SetSource(std::shared_ptr<AudioSource> src)
{
    source = src;
}

void AudioControl::OnAdded()
{
    if (source != nullptr) {
        source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnRemoved()
{
    if (source != nullptr) {
        source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnUpdate(double dt)
{
    auto &current_position = parent->GetGlobalTransform().GetTranslation();
    if (source != nullptr && last_position != current_position) {
        source->SetPosition(current_position);
        last_position = current_position;
    }
}
}