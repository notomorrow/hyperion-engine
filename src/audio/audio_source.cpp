#include "audio_source.h"
#include "audio_manager.h"

namespace hyperion {
AudioSource::AudioSource(int format, unsigned char *data, size_t size, size_t freq)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alGenBuffers(1, &m_buffer_id);
        alBufferData(m_buffer_id, format, (void*)data, size, freq);

        alGenSources(1, &m_source_id);
        alSourcei(m_source_id, AL_BUFFER, m_buffer_id);
    }
}

AudioSource::~AudioSource()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        Stop();
        alDeleteSources(1, &m_source_id);
        alDeleteBuffers(1, &m_buffer_id);
    }
}

void AudioSource::SetPosition(const Vector3 &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(m_source_id, AL_POSITION, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetVelocity(const Vector3 &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(m_source_id, AL_VELOCITY, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetPitch(float pitch)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(m_source_id, AL_PITCH, pitch);
    }
}

void AudioSource::SetGain(float gain)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(m_source_id, AL_GAIN, gain);
    }
}

void AudioSource::SetLoop(bool loop)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcei(m_source_id, AL_LOOPING, loop);
    }
}

void AudioSource::Play()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePlay(m_source_id);
    }
}

void AudioSource::Pause()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePause(m_source_id);
    }
}

void AudioSource::Stop()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourceStop(m_source_id);
    }
}
} // namespace hyperion
