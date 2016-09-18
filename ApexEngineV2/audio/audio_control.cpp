#include "audio_control.h"
#include "../entity.h"

namespace apex {
AudioControl::AudioControl(std::shared_ptr<AudioSource> source)
    : EntityControl(10), m_source(source)
{
}

void AudioControl::OnAdded()
{
    if (m_source != nullptr) {
        m_source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnRemoved()
{
    if (m_source != nullptr) {
        m_source->SetPosition(Vector3::Zero());
    }
}

void AudioControl::OnUpdate(double dt)
{
    auto &current_position = parent->GetGlobalTransform().GetTranslation();
    if (m_source != nullptr && m_last_position != current_position) {
        m_source->SetPosition(current_position);
        m_last_position = current_position;
    }
}
} // namespace apex