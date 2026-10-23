/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Reflection/Property.hpp>
#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Method.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Reflection/TypeInfo.hpp>

namespace Hyperion {

const Class* Property::GetClass() const
{
    if (!m_typeInfo)
    {
        return nullptr;
    }

    return m_typeInfo->GetClass();
}

CORE_API bool CanGetAndSetMember(const IMember& member)
{
    switch (member.GetMemberType())
    {
    case MemberType::Field:
        return true;
    case MemberType::Property:
    {
        const Property& property = static_cast<const Property&>(member);

        return property.CanGet() && property.CanSet();
    }
    default:
        return false;
    }
}

CORE_API BoxedValue GetMemberValue(const IMember& member, const BoxedValue& target)
{
    switch (member.GetMemberType())
    {
    case MemberType::Field:
        return static_cast<const Field&>(member).Get(target);
    case MemberType::Property:
        return static_cast<const Property&>(member).Get(target);
    default:
        return BoxedValue();
    }
}

CORE_API void SetMemberValue(const IMember& member, BoxedValue& target, const BoxedValue& value)
{
    switch (member.GetMemberType())
    {
    case MemberType::Field:
        static_cast<const Field&>(member).Set(target, value);
        break;
    case MemberType::Property:
        static_cast<const Property&>(member).Set(target, value);
        break;
    default:
        break;
    }
}

} // namespace Hyperion
