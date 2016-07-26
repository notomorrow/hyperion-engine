#include "audio_source.h"
#include "audio_manager.h"

namespace apex {
AudioSource::AudioSource(int format, unsigned char *data, size_t size, size_t freq)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alGenBuffers(1, &buffer);
        alBufferData(buffer, format, (void*)data, size, freq);

        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer);
    }
}

AudioSource::~AudioSource()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        Stop();
        alDeleteSources(1, &source);
        alDeleteBuffers(1, &buffer);
    }
}

void AudioSource::SetPosition(const Vector3 &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(source, AL_POSITION, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetVelocity(const Vector3 &vec)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSource3f(source, AL_VELOCITY, vec.x, vec.y, vec.z);
    }
}

void AudioSource::SetPitch(float pitch)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(source, AL_PITCH, pitch);
    }
}

void AudioSource::SetGain(float gain)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcef(source, AL_GAIN, gain);
    }
}

void AudioSource::SetLoop(bool loop)
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcei(source, AL_LOOPING, loop);
    }
}

void AudioSource::Play()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePlay(source);
    }
}

void AudioSource::Pause()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourcePause(source);
    }
}

void AudioSource::Stop()
{
    if (AudioManager::GetInstance()->IsInitialized()) {
        alSourceStop(source);
    }
}
}