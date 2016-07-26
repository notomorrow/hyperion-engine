#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include "../asset/loadable.h"
#include "../math/vector3.h"

namespace apex {
class AudioSource : public Loadable {
public:
    AudioSource(int format, unsigned char *data, size_t size, size_t freq);
    ~AudioSource();

    void SetPosition(const Vector3 &vec);
    void SetVelocity(const Vector3 &vec);
    void SetPitch(float pitch);
    void SetGain(float gain);
    void SetLoop(bool loop);

    void Play();
    void Pause();
    void Stop();
    
private:
    unsigned int buffer, source;
};
}

#endif