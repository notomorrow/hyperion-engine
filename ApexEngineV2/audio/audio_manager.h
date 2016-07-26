#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <AL/al.h>
#include <AL/alc.h>

#include "../math/vector3.h"

namespace apex {
class AudioManager {
public:
    static AudioManager *GetInstance();

    AudioManager();
    ~AudioManager();

    bool Initialize();
    bool IsInitialized() const;
    void ListDevices();
    ALCdevice *GetDevice();
    ALCcontext *GetContext();

    void SetListenerPosition(const Vector3 &vec);
    void SetListenerOrientation(const Vector3 &forward, const Vector3 &up);

private:
    static AudioManager *instance;

    bool is_initialized;

    ALCdevice *dev;
    ALCcontext *ctx;
};
}

#endif