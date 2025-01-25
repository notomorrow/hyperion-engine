/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CONSTANT_HPP
#define HYPERION_CORE_HYP_CONSTANT_HPP

#include <core/object/HypData.hpp>
#include <core/object/HypClassAttribute.hpp>
#include <core/object/HypMemberFwd.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/functional/Proc.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Span.hpp>

#include <core/containers/String.hpp>

#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>

namespace hyperion {

class HypClass;

class HypConstant : public IHypMember
{
public:
    template <class ConstantType>
    HypConstant(Name name, ConstantType value, Span<const HypClassAttribute> attributes = { })
        : m_name(name),
          m_type_id(TypeID::ForType<ConstantType>()),
          m_size(sizeof(ConstantType)),
          m_attributes(attributes)
    {
        m_get_proc = [value]() -> HypData
        {
            return HypData(value);
        };

        if (m_attributes["serialize"] || m_attributes["xmlattribute"]) {
            m_serialize_proc = [value]() -> fbom::FBOMData
            {
                fbom::FBOMData out;

                if (fbom::FBOMResult err = HypDataHelper<NormalizedType<ConstantType>>::Serialize(value, out)) {
                    HYP_FAIL("Failed to serialize data: %s", err.message.Data());
                }

                return out;
            };
        }
    }

    HypConstant(const HypConstant &other)                   = delete;
    HypConstant &operator=(const HypConstant &other)        = delete;
    HypConstant(HypConstant &&other) noexcept               = default;
    HypConstant &operator=(HypConstant &&other) noexcept    = default;
    virtual ~HypConstant() override                         = default;

    virtual HypMemberType GetMemberType() const override
    {
        return HypMemberType::TYPE_CONSTANT;
    }

    virtual Name GetName() const override
    {
        return m_name;
    }

    virtual TypeID GetTypeID() const override
    {
        return m_type_id;
    }

    virtual TypeID GetTargetTypeID() const override
    {
        return TypeID::Void();
    }
    
    virtual const HypClassAttributeSet &GetAttributes() const override
    {
        return m_attributes;
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key) const override
    {
        return m_attributes.Get(key);
    }

    virtual const HypClassAttributeValue &GetAttribute(ANSIStringView key, const HypClassAttributeValue &default_value) const override
    {
        return m_attributes.Get(key, default_value);
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return IsValid(); }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_name.IsValid()
            && m_type_id != TypeID::Void()
            && m_size != 0;
    }

    HYP_FORCE_INLINE bool CanSerialize() const
        { return IsValid() && m_serialize_proc.IsValid(); }

    HYP_FORCE_INLINE fbom::FBOMData Serialize() const
    {
        AssertThrow(CanSerialize());

        return m_serialize_proc();
    }

    HYP_FORCE_INLINE HypData Get() const
    {
        return m_get_proc();
    }

private:
    Name                    m_name;
    TypeID                  m_type_id;
    uint32                  m_size;
    HypClassAttributeSet    m_attributes;

    Proc<HypData>           m_get_proc;
    Proc<fbom::FBOMData>    m_serialize_proc;
};

} // namespace hyperion

#endif