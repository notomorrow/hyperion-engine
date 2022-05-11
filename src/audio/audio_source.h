#ifndef AUDIO_SOURCE_H
#define AUDIO_SOURCE_H

#include "../math/vector3.h"

namespace hyperion {
class AudioSource {
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
    unsigned int m_buffer_id;
    unsigned int m_source_id;
};
} // namespace hyperion

#endif
