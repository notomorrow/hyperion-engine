#ifndef HYPERION_V2_FBOM_MARSHALS_SUB_SHADER_MARSHAL_HPP
#define HYPERION_V2_FBOM_MARSHALS_SUB_SHADER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Shader.hpp>
#include <Engine.hpp>

namespace hyperion::v2::fbom {

template <>
class FBOMMarshaler<SubShader> : public FBOMObjectMarshalerBase<SubShader>
{
public:
    virtual ~FBOMMarshaler() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType("SubShader");
    }

    virtual FBOMResult Serialize(const SubShader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("type", FBOMUnsignedInt(), static_cast<UInt32>(in_object.type));

        out.SetProperty(
            "bytes",
            FBOMArray(FBOMByte(), in_object.spirv.bytes.Size()),
            in_object.spirv.bytes.Data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(Engine *, const FBOMObject &in, UniquePtr<SubShader> &out_object) const override
    {
        out_object.Reset(new SubShader());

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&out_object->type)) {
            return err;
        }

        if (const auto &bytes_property = in.GetProperty("bytes")) {
            const auto num_bytes = bytes_property.NumArrayElements(FBOMByte());

            if (num_bytes != 0) {
                if (auto err = bytes_property.ReadBytes(num_bytes, out_object->spirv.bytes)) {
                    return err;
                }
            }
        }

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::v2::fbom

#endif