#include "Type.hpp"
#include "BaseTypes.hpp"

namespace hyperion::v2::fbom {

FBOMType FBOMType::Extend(const FBOMType &object) const
{
    return FBOMObjectType(object.name, *this);
}

} // namespace hyperion::v2::fbom
