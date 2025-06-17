/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/serialization/fbom/marshals/HypClassInstanceMarshal.hpp>
#include <core/serialization/fbom/FBOMLoadContext.hpp>

#include <core/object/HypClass.hpp>
#include <core/object/HypStruct.hpp>
#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/Node.hpp>                         // temp
#include <scene/Entity.hpp>                       // temp
#include <scene/ecs/components/MeshComponent.hpp> // temp

namespace hyperion::serialization {

FBOMResult HypClassInstanceMarshal::Serialize(ConstAnyRef in, FBOMObject& out) const
{
    if (!in.HasValue())
    {
        return { FBOMResult::FBOM_ERR, "Attempting to serialize null object" };
    }

    const HypClass* hypClass = GetClass(in.GetTypeId());

    if (!hypClass)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", in.GetTypeId().Value()) };
    }

    if (!hypClass->CanSerialize())
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object using HypClassInstanceMarshal, TypeId {} has no associated HypClass", in.GetTypeId().Value()) };
    }

    const HypClassAttributeValue& serializeAttribute = hypClass->GetAttribute("serialize");

    if (serializeAttribute == false)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=false", hypClass->GetName()) };
    }

    HYP_NAMED_SCOPE_FMT("Serializing object with HypClass '{}'", hypClass->GetName());

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        if (FBOMResult err = hypStruct->SerializeStruct(in, out))
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}': {}", hypClass->GetName(), err.message) };
        }

        return { FBOMResult::FBOM_OK };
    }

    HypData targetData { AnyRef { in.GetTypeId(), const_cast<void*>(in.GetPointer()) } };

    out = FBOMObject(FBOMObjectType(hypClass));

    {
        HYP_NAMED_SCOPE_FMT("Serializing properties for HypClass '{}'", hypClass->GetName());

        for (IHypMember& member : hypClass->GetMembers(HypMemberType::TYPE_PROPERTY))
        {
            if (!member.CanSerialize())
            {
                continue;
            }

            if (!member.GetAttribute("serialize"))
            {
                continue;
            }

            HYP_NAMED_SCOPE_FMT("Serializing member '{}' for HypClass '{}'", member.GetName(), hypClass->GetName());

            FBOMData data;

            if (!member.Serialize(Span<HypData>(&targetData, 1), data))
            {
                return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to serialize member '{}' of HypClass '{}'", member.GetName(), hypClass->GetName()) };
            }

            out.SetProperty(member.GetName().LookupString(), std::move(data));
        }
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const
{
    const HypClass* hypClass = in.GetHypClass();

    if (!hypClass)
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, serialized data with type '{}' (TypeId: {}) has no associated HypClass", in.GetType().name, in.GetType().GetNativeTypeId().Value()) };
    }

    if (!hypClass->CreateInstance(out))
    {
        return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot deserialize object using HypClassInstanceMarshal, HypClass '{}' instance creation failed", hypClass->GetName()) };
    }

    if (hypClass->GetSerializationMode() & HypClassSerializationMode::BITWISE)
    {
        if (!hypClass->IsStructType())
        {
            return { FBOMResult::FBOM_ERR, HYP_FORMAT("Cannot serialize object with HypClass '{}', HypClass has attribute \"serialize\"=\"bitwise\" but is not a struct type", hypClass->GetName()) };
        }

        const HypStruct* hypStruct = static_cast<const HypStruct*>(hypClass);

        return hypStruct->DeserializeStruct(context, in, out);
    }

    AnyRef ref = out.ToRef();
    AssertThrowMsg(ref.HasValue(), "Failed to create HypClass instance");

    if (FBOMResult err = Deserialize_Internal(context, in, hypClass, ref))
    {
        return err;
    }

    return { FBOMResult::FBOM_OK };
}

FBOMResult HypClassInstanceMarshal::Deserialize_Internal(FBOMLoadContext& context, const FBOMObject& in, const HypClass* hypClass, AnyRef ref) const
{
    AssertThrow(hypClass != nullptr);
    AssertThrow(ref.HasValue());

    HypData targetData { ref };

    {
        HYP_NAMED_SCOPE_FMT("Deserializing properties for HypClass '{}'", hypClass->GetName());

        // temp
        if (hypClass->GetName() == NAME("Scene"))
        {
            HYP_LOG(Serialization, Debug, "Deserializing properties for HypClass '{}'", hypClass->GetName());
        }

        for (const KeyValuePair<ANSIString, FBOMData>& it : in.GetProperties())
        {
            if (const HypProperty* property = hypClass->GetProperty(it.first))
            {
                if (!property->GetAttribute("serialize"))
                {
                    continue;
                }

                if (!property->CanDeserialize())
                {
                    HYP_NAMED_SCOPE_FMT("Deserializing member '{}' on object, skipping setter for HypClass '{}'", property->GetName(), hypClass->GetName());

                    continue;
                }

                // temp
                if (property->GetName() == NAME("Entity") && hypClass == Node::Class())
                {
                    HYP_LOG(Serialization, Debug, "Setting entity for Node with Id : {}, and name: {}", ref.Get<Node>().Id(), ref.Get<Node>().GetName());
                }

                if (hypClass->GetName() == NAME("MeshComponent"))
                {
                    HYP_LOG(Serialization, Debug, "Deserializing property '{}' for HypClass '{}'",
                        property->GetName(), hypClass->GetName());
                }

                if (!property->Deserialize(context, targetData, it.second))
                {
                    return { FBOMResult::FBOM_ERR, HYP_FORMAT("Failed to deserialize member '{}' of HypClass '{}'", property->GetName(), hypClass->GetName()) };
                }
            }
        }
    }

    // temp
    if (hypClass->GetName() == NAME("MeshComponent"))
    {
        AssertThrowMsg(ref.Get<MeshComponent>().mesh != nullptr,
            "MeshComponent deserialized with null mesh, this is likely due to a missing 'mesh' property in the FBOM data");
    }

    hypClass->PostLoad(ref.GetPointer());

    return { FBOMResult::FBOM_OK };
}

} // namespace hyperion::serialization