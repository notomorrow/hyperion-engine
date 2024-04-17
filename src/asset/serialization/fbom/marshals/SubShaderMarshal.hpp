/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_MARSHALS_SUB_SHADER_MARSHAL_HPP
#define HYPERION_FBOM_MARSHALS_SUB_SHADER_MARSHAL_HPP

#include <asset/serialization/fbom/FBOM.hpp>
#include <rendering/Shader.hpp>
#include <Engine.hpp>

namespace hyperion::fbom {

template <>
class FBOMMarshaler<SubShader> : public FBOMObjectMarshalerBase<SubShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const SubShader &in_object, FBOMObject &out) const override
    {
        out.SetProperty("type", FBOMUnsignedInt(), static_cast<uint32>(in_object.type));

        out.SetProperty(
            "bytes",
            FBOMSequence(FBOMByte(), in_object.spirv.bytes.Size()),
            in_object.spirv.bytes.Data()
        );

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(const FBOMObject &in, UniquePtr<void> &out_object) const override
    {
        auto sub_shader = UniquePtr<SubShader>::Construct();

        if (auto err = in.GetProperty("type").ReadUnsignedInt(&sub_shader->type)) {
            return err;
        }

        if (const auto &bytes_property = in.GetProperty("bytes")) {
            const auto num_bytes = bytes_property.NumElements(FBOMByte());

            if (num_bytes != 0) {
                if (auto err = bytes_property.ReadBytes(num_bytes, sub_shader->spirv.bytes)) {
                    return err;
                }
            }
        }

        out_object = std::move(sub_shader);

        return { FBOMResult::FBOM_OK };
    }
};

} // namespace hyperion::fbom

#endif