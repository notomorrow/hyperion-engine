/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Mesh.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Mesh> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const Mesh& inObject = in.Get<Mesh>();

        out.SetProperty("Topology", uint32(inObject.GetTopology()));
        out.SetProperty("Attributes", FBOMStruct::Create<VertexAttributeSet>(), sizeof(VertexAttributeSet), &inObject.GetVertexAttributes());

        /// FIXME: StreamedMeshData is no longer HypObject
        StreamedMeshData* streamedMeshData = inObject.GetStreamedMeshData();

        if (streamedMeshData != nullptr)
        {
            ResourceHandle resourceHandle(*streamedMeshData);

            const MeshData& meshData = streamedMeshData->GetMeshData();

            if (FBOMResult err = out.AddChild(meshData))
            {
                return err;
            }
        }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Topology topology = TOP_TRIANGLES;

        if (FBOMResult err = in.GetProperty("Topology").ReadUInt32(&topology))
        {
            return err;
        }

        VertexAttributeSet vertexAttributes;

        if (FBOMResult err = in.GetProperty("Attributes").ReadStruct(&vertexAttributes))
        {
            return err;
        }

        RC<StreamedMeshData> streamedMeshData;
        ResourceHandle streamedMeshDataResourceHandle;

        const auto meshDataIt = in.GetChildren().FindIf([](const FBOMObject& item)
            {
                return item.GetType().IsOrExtends("MeshData");
            });

        if (meshDataIt != in.GetChildren().End())
        {
            streamedMeshData.EmplaceAs<StreamedMeshData>(meshDataIt->m_deserializedObject->Get<MeshData>(), streamedMeshDataResourceHandle);
        }

        Handle<Mesh> meshHandle = CreateObject<Mesh>(
            std::move(streamedMeshData),
            topology,
            vertexAttributes);

        streamedMeshDataResourceHandle.Reset();

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, Mesh::Class(), AnyRef(*meshHandle)))
        {
            return err;
        }

        out = HypData(std::move(meshHandle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Mesh, FBOMMarshaler<Mesh>);

} // namespace hyperion::serialization