/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <core/object/HypData.hpp>

#include <rendering/ShaderManager.hpp>

namespace hyperion::serialization {

#if 0

template <>
class FBOMMarshaler<SubShader> : public FBOMObjectMarshalerBase<SubShader>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const SubShader &inObject, FBOMObject &out) const override
    {
        out.SetProperty("Type", FBOMData::FromUInt32(inObject.type));
        out.SetProperty("Bytes", FBOMData::FromByteBuffer(inObject.spirv.bytes, FBOMDataFlags::COMPRESSED));

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext &context, const FBOMObject &in, HypData &out) const override
    {
        SubShader subShader;

        if (FBOMResult err = in.GetProperty("Type").ReadUInt32(&subShader.type)) {
            return err;
        }

        if (const FBOMData &bytesProperty = in.GetProperty("Bytes")) {
            if (FBOMResult err = bytesProperty.ReadByteBuffer(subShader.spirv.bytes)) {
                return err;
            }
        }

        out = HypData(std::move(subShader));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(SubShader, FBOMMarshaler<SubShader>);

#endif

} // namespace hyperion::serialization