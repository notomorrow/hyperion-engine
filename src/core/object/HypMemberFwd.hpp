/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_MEMBER_FWD_HPP
#define HYPERION_CORE_HYP_MEMBER_FWD_HPP

#include <core/Defines.hpp>
#include <core/Name.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/TypeID.hpp>

namespace hyperion {

class HypClassAttributeSet;
class HypClassAttributeValue;
struct HypData;

namespace fbom {

class FBOMData;
class FBOMLoadContext;

} // namespace fbom

enum class HypMemberType : uint8
{
    NONE = 0x0,
    TYPE_FIELD = 0x1,
    TYPE_METHOD = 0x2,
    TYPE_PROPERTY = 0x4,
    TYPE_CONSTANT = 0x8
};

HYP_MAKE_ENUM_FLAGS(HypMemberType)

class IHypMember
{
public:
    virtual ~IHypMember() = default;

    virtual HypMemberType GetMemberType() const = 0;

    virtual Name GetName() const = 0;

    virtual TypeID GetTypeID() const = 0;

    virtual TypeID GetTargetTypeID() const = 0;

    virtual bool CanSerialize() const = 0;
    virtual bool CanDeserialize() const = 0;

    virtual bool Serialize(Span<HypData> args, fbom::FBOMData& out) const = 0;
    virtual bool Deserialize(fbom::FBOMLoadContext& context, HypData& target, const fbom::FBOMData& value) const = 0;

    virtual const HypClassAttributeSet& GetAttributes() const = 0;
    virtual const HypClassAttributeValue& GetAttribute(ANSIStringView key) const = 0;
    virtual const HypClassAttributeValue& GetAttribute(ANSIStringView key, const HypClassAttributeValue& default_value) const = 0;
};

} // namespace hyperion

#endif