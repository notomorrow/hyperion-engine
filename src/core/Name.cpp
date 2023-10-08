#include <core/Name.hpp>

namespace hyperion {

const Name Name::invalid = Name(0);

NameRegistry *Name::GetRegistry()
{
    static NameRegistry registry;

    return &registry;
}

const ANSIString &Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this);
}

Name CreateNameFromDynamicString(const ANSIString &str)
{
    const NameRegistration name_registration = NameRegistration::FromDynamicString(str);

    return Name(name_registration.id);
}

} // namespace hyperion::v2