/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/object/HypProperty.hpp>
#include <core/object/HypField.hpp>
#include <core/object/HypMethod.hpp>
#include <core/object/HypClassRegistry.hpp>

namespace hyperion {

const HypClass* HypProperty::GetHypClass() const
{
    return HypClassRegistry::GetInstance().GetClass(m_typeId);
}

HypProperty HypProperty::MakeHypProperty(const HypField* field)
{
    HYP_CORE_ASSERT(field != nullptr);

    Name propertyName;

    if (const HypClassAttributeValue& attr = field->GetAttribute("property"); attr.IsString())
    {
        propertyName = CreateNameFromDynamicString(attr.GetString());
    }
    else
    {
        propertyName = field->GetName();
    }

    HypProperty result;
    result.m_name = propertyName;
    result.m_typeId = field->GetTypeId();
    result.m_attributes = field->GetAttributes();

    result.m_getter = HypPropertyGetter();
    result.m_getter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_getter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_getter.getProc = [field](const HypData& target) -> HypData
    {
        return field->Get(target);
    };
    result.m_getter.serializeProc = [field](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
    {
        FBOMData data;

        if (!field->Serialize(target, data, flags))
        {
            return FBOMData();
        }

        return data;
    };

    result.m_setter = HypPropertySetter();
    result.m_setter.typeInfo.targetTypeId = field->GetTargetTypeId();
    result.m_setter.typeInfo.valueTypeId = field->GetTypeId();
    result.m_setter.setProc = [field](HypData& target, const HypData& value) -> void
    {
        field->Set(target, value);
    };
    result.m_setter.deserializeProc = [field](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> void
    {
        field->Deserialize(context, target, value);
    };

    result.m_originalMember = field;

    return result;
}

HypProperty HypProperty::MakeHypProperty(const HypMethod* getter, const HypMethod* setter)
{
    HypProperty result;

    Optional<String> propertyAttributeOpt;

    Optional<TypeId> typeId;
    Optional<TypeId> targetTypeId;

    const bool hasGetter = getter != nullptr && getter->GetParameters().Size() >= 1;
    const bool hasSetter = setter != nullptr && setter->GetParameters().Size() >= 2;

    if (hasGetter)
    {
        if (const HypClassAttributeValue& attr = getter->GetAttribute("property"))
        {
            propertyAttributeOpt = attr.GetString();
        }

        typeId = getter->GetTypeId();
        targetTypeId = getter->GetParameters()[0].typeId;

        result.m_attributes = getter->GetAttributes();
    }

    if (hasSetter)
    {
        if (!propertyAttributeOpt)
        {
            if (const HypClassAttributeValue& attr = setter->GetAttribute("property"))
            {
                propertyAttributeOpt = attr.GetString();
            }
        }

        const TypeId setterTypeId = setter->GetParameters()[0].typeId;

        if (typeId.HasValue())
        {
            HYP_CORE_ASSERT(*typeId == setterTypeId, "Getter TypeId (%u) does not match setter TypeId (%u)", typeId->Value(), setterTypeId.Value());
        }
        else
        {
            typeId = setterTypeId;
        }

        if (!typeId.HasValue())
        {
            typeId = setterTypeId;
        }

        if (targetTypeId.HasValue())
        {
            HYP_CORE_ASSERT(*targetTypeId == setter->GetTargetTypeId(), "Getter target TypeId (%u) does not match setter target TypeId (%u)", targetTypeId->Value(), setter->GetTargetTypeId().Value());
        }
        else
        {
            targetTypeId = setter->GetTargetTypeId();
        }

        result.m_attributes.Merge(setter->GetAttributes());
    }

    HYP_CORE_ASSERT(propertyAttributeOpt.HasValue(), "A HypProperty composed of getter/setter pair must have at least one method that has \"Property=\" attribute");
    HYP_CORE_ASSERT(typeId.HasValue(), "Cannot determine TypeId from getter/setter pair");

    result.m_name = CreateNameFromDynamicString(*propertyAttributeOpt);
    result.m_typeId = *typeId;

    if (hasGetter)
    {
        result.m_getter = HypPropertyGetter();
        result.m_getter.typeInfo.targetTypeId = *targetTypeId;
        result.m_getter.typeInfo.valueTypeId = *typeId;
        result.m_getter.getProc = [getter](const HypData& target) -> HypData
        {
            return getter->Invoke(Span<HypData> { const_cast<HypData*>(&target), 1 });
        };
        result.m_getter.serializeProc = [getter](const HypData& target, EnumFlags<FBOMDataFlags> flags) -> FBOMData
        {
            FBOMData data;
            
            const bool result = getter->Serialize(Span<HypData> { const_cast<HypData*>(&target), 1 }, data, flags);
            
            HYP_CORE_ASSERT(result);

            return data;
        };
        result.m_originalMember = getter;
    }

    if (hasSetter)
    {
        result.m_setter = HypPropertySetter();
        result.m_setter.typeInfo.targetTypeId = *targetTypeId;
        result.m_setter.typeInfo.valueTypeId = setter->GetParameters()[0].typeId;
        result.m_setter.setProc = [setter](HypData& target, const HypData& value) -> void
        {
            setter->Invoke(Span<HypData*> { { &target, const_cast<HypData*>(&value) } });
        };
        result.m_setter.deserializeProc = [setter](FBOMLoadContext& context, HypData& target, const FBOMData& value) -> void
        {
            const bool result = setter->Deserialize(context, target, value);
            
            HYP_CORE_ASSERT(result);
        };
        result.m_originalMember = setter;
    }

    return result;
}

} // namespace hyperion
