#ifndef HYPERION_V2_WAV_AUDIO_LOADER_H
#define HYPERION_V2_WAV_AUDIO_LOADER_H

#include <asset/Assets.hpp>
#include <audio/AudioSource.hpp>
#include <core/Containers.hpp>

namespace hyperion::v2 {

class WAVAudioLoader : public AssetLoader
{
public:

    struct WAVAudio
    {
        struct RiffHeader
        {
            UInt8 chunk_id[4];
            UInt32 chunk_size;
            UInt8 format[4];
        } riff_header;

        struct WaveFormat
        {
            UInt8 sub_chunk_id[4];
            UInt32 sub_chunk_size;
            UInt16 audio_format;
            UInt16 num_channels;
            UInt32 sample_rate;
            UInt32 byte_rate;
            UInt16 block_align;
            UInt16 bits_per_sample;
        } wave_format;

        struct WaveData
        {
            UInt8 sub_chunk_id[4];
            UInt32 sub_chunk_2_size;
        } wave_data;

        std::vector<UInt8> wave_bytes;
        SizeType size;
        SizeType frequency;

        AudioSource::Format format;
    };

    virtual ~WAVAudioLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif
