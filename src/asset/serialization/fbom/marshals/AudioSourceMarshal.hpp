#ifndef HYPERION_V2_FBOM_MARSHALS_AUDIO_SOURCE_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_AUDIO_SOURCE_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <audio/AudioSource.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<AudioSource> : public FBOMObjectMarshalerBase<AudioSource>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(AudioSource::GetClass().GetName());
    }

    virtual FBOMResult Serialize(const AudioSource &in_object, FBOMObject &out) const override
    {
        out.SetProperty("format", FBOMUnsignedInt(), in_object.GetFormat());
        out.SetProperty("byte_buffer", in_object.GetByteBuffer());
        out.SetProperty("freq", FBOMUnsignedLong(), in_object.GetFreq());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        ByteBuffer byte_buffer;

        if (auto err = in.GetProperty("byte_buffer").ReadByteBuffer(byte_buffer)) {
            return err;
        }

        UInt format;

        if (auto err = in.GetProperty("format").ReadUnsignedInt(&format)) {
            return err;
        }

        SizeType freq;

        if (auto err = in.GetProperty("freq").ReadUnsignedLong(&freq)) {
            return err;
        }

        out_object = UniquePtr<Handle<AudioSource>>::Construct(CreateObject<AudioSource>(
            AudioSource::Format(format),
            byte_buffer,
            freq
        ));

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif