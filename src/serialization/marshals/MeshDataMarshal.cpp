/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Mesh.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<MeshData> : public FBOMObjectMarshalerBase<MeshData>
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(const MeshData& inObject, FBOMObject& out) const override
    {
        out.SetProperty(
            "Vertices",
            FBOMSequence(FBOMStruct::Create<Vertex>(), inObject.vertices.Size()),
            inObject.vertices.ByteSize(),
            inObject.vertices.Data());

        out.SetProperty(
            "Indices",
            FBOMSequence(FBOMUInt32(), inObject.indices.Size()),
            inObject.indices.ByteSize(),
            inObject.indices.Data());

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Array<Vertex> vertices;

        if (const FBOMData& verticesProperty = in.GetProperty("Vertices"))
        {
            const SizeType numVertices = verticesProperty.NumElements(FBOMStruct::Create<Vertex>());

            if (numVertices != 0)
            {
                vertices.Resize(numVertices);

                if (FBOMResult err = verticesProperty.ReadElements(FBOMStruct::Create<Vertex>(), numVertices, vertices.Data()))
                {
                    return err;
                }
            }
        }
        else
        {
            return { FBOMResult::FBOM_ERR, String("vertices property invalid on object ") + in.ToString(false) };
        }

        Array<uint32> indices;

        if (const FBOMData& indicesProperty = in.GetProperty("Indices"))
        {
            const SizeType numIndices = indicesProperty.NumElements(FBOMUInt32());

            if (numIndices != 0)
            {
                indices.Resize(numIndices);

                if (FBOMResult err = indicesProperty.ReadElements(FBOMUInt32(), numIndices, indices.Data()))
                {
                    return err;
                }
            }
        }
        else
        {
            return { FBOMResult::FBOM_ERR, "indices property invalid" };
        }

        out = HypData(MeshData {
            std::move(vertices),
            std::move(indices) });

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(MeshData, FBOMMarshaler<MeshData>);

} // namespace hyperion::serialization