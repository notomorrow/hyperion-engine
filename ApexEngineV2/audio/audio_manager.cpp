#include "audio_manager.h"

#include <iostream>

namespace apex {
AudioManager *AudioManager::instance = nullptr;

AudioManager *AudioManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new AudioManager();
    }
    return instance;
}

AudioManager::AudioManager()
{
    is_initialized = false;
}

AudioManager::~AudioManager()
{
    if (is_initialized) {
        alcCloseDevice(dev);
        alcDestroyContext(ctx);
    }
    is_initialized = false;
}

bool AudioManager::Initialize()
{
    dev = alcOpenDevice(NULL);
    if (!dev) {
        std::cout << "Failed to open OpenAL device\n";
        is_initialized = false;
        return false;
    }

    ctx = alcCreateContext(dev, NULL);
    alcMakeContextCurrent(ctx);
    if (!ctx) {
        std::cout << "Failed to open OpenAL context\n";
        is_initialized = false;
        return false;
    }

    ALfloat orientation[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    alListenerfv(AL_ORIENTATION, orientation);

    is_initialized = true;
    return true;
}

bool AudioManager::IsInitialized() const
{
    return is_initialized;
}

void AudioManager::ListDevices()
{
    const ALCchar *device = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar *next = device + 1;
    size_t len = 0;

    std::cout << "Devices list:\n";
    std::cout << "----------\n";
    while (device && *device != '\0' && next && *next != '\0') {
        std::cout << device << "\n";
        len = strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
    std::cout << "----------\n";
}

ALCdevice *AudioManager::GetDevice()
{
    return dev;
}

ALCcontext *AudioManager::GetContext()
{
    return ctx;
}

void AudioManager::SetListenerPosition(const Vector3 &vec)
{
    alListener3f(AL_POSITION, vec.x, vec.y, vec.z);
}

void AudioManager::SetListenerOrientation(const Vector3 &forward, const Vector3 &up)
{
    const float values[] = { forward.x, forward.y, forward.z, up.x, up.y, up.z };
    alListenerfv(AL_ORIENTATION, values);
}
}