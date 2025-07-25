/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/FBOM.hpp>
#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>

#include <core/object/HypData.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Texture.hpp>

#include <streaming/StreamedTextureData.hpp>

#include <rendering/RenderImage.hpp>

namespace hyperion::serialization {

template <>
class FBOMMarshaler<Texture> : public HypClassInstanceMarshal
{
public:
    virtual ~FBOMMarshaler() override = default;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override
    {
        if (FBOMResult err = HypClassInstanceMarshal::Serialize(in, out))
        {
            return err;
        }

        const Texture& inObject = in.Get<Texture>();

        // /// FIXME: StreamedTextureData is no longer HypObject
        // if (const RC<StreamedTextureData>& streamedTextureData = inObject.GetStreamedTextureData())
        // {
        //     TResourceHandle<StreamedTextureData> resourceHandle(*streamedTextureData);

        //     const TextureData& textureData = resourceHandle->GetTextureData();

        //     if (textureData.buffer.Empty())
        //     {
        //         HYP_LOG(Serialization, Warning, "TextureData for Texture '{}' is empty!", inObject.GetName());
        //     }
        //     else if (textureData.buffer.Size() != inObject.GetTextureDesc().GetByteSize())
        //     {
        //         HYP_LOG(Serialization, Warning, "TextureData for Texture '{}' has size {}, but TextureDesc expects size {}!",
        //             inObject.GetName(), textureData.buffer.Size(), inObject.GetTextureDesc().GetByteSize());
        //     }
        //     else
        //     {
        //         out.AddChild(textureData);
        //     }
        // }
        // else
        // {
        //     const TextureDesc& desc = inObject.GetTextureDesc();

        //     out.AddChild(desc);
        // }

        return { FBOMResult::FBOM_OK };
    }

    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override
    {
        Handle<Texture> textureHandle;

        // RC<StreamedTextureData> streamedTextureData;
        // ResourceHandle streamedTextureDataResourceHandle;
        // TextureDesc textureDesc;

        // const auto textureDataIt = in.GetChildren().FindIf([](const FBOMObject& item)
        //     {
        //         return item.GetType().IsOrExtends("TextureData");
        //     });

        // if (textureDataIt != in.GetChildren().End())
        // {
        //     streamedTextureData = MakeRefCountedPtr<StreamedTextureData>(
        //         textureDataIt->m_deserializedObject->Get<TextureData>(),
        //         streamedTextureDataResourceHandle);
        // }

        // const auto textureDescIt = in.GetChildren().FindIf([](const FBOMObject& item)
        //     {
        //         return item.GetType().IsOrExtends("TextureDesc");
        //     });

        // if (textureDescIt != in.GetChildren().End())
        // {
        //     textureDesc = textureDescIt->m_deserializedObject->Get<TextureDesc>();
        // }

        // if (streamedTextureData != nullptr)
        // {
        //     textureHandle = CreateObject<Texture>(streamedTextureData);
        //     streamedTextureDataResourceHandle.Reset();
        // }
        // else
        // {
        //     textureHandle = CreateObject<Texture>(textureDesc);
        // }

        // if (!textureHandle.IsValid())
        // {
        //     return { FBOMResult::FBOM_ERR, "No TextureData or TextureDesc on Texture object" };
        // }

        if (FBOMResult err = HypClassInstanceMarshal::Deserialize_Internal(context, in, Texture::Class(), AnyRef(*textureHandle)))
        {
            return err;
        }

        out = HypData(std::move(textureHandle));

        return { FBOMResult::FBOM_OK };
    }
};

HYP_DEFINE_MARSHAL(Texture, FBOMMarshaler<Texture>);

} // namespace hyperion::serialization