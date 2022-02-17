#include "entity_control.h"

namespace hyperion {

EntityControl::EntityControl(const fbom::FBOMType &loadable_type, const double tps)
    : Control(fbom::FBOMObjectType("ENTITY_CONTROL").Extend(loadable_type), tps)
{
}

} // namespace hyperion
