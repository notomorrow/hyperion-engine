#include <core/Name.hpp>

namespace hyperion::v2 {

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

} // namespace hyperion::v2