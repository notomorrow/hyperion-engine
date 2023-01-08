#include <core/Name.hpp>

namespace hyperion::v2 {

const Name Name::invalid = Name(0);

UniquePtr<NameRegistry> Name::registry = UniquePtr<NameRegistry>::Construct();

NameRegistry *Name::GetRegistry()
{
    return registry.Get();
}

const ANSIString &Name::LookupString() const
{
    return GetRegistry()->LookupStringForName(*this);
}

} // namespace hyperion::v2