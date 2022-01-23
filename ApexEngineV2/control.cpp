#include "control.h"

namespace hyperion {

EntityControl::EntityControl(const double tps)
    : tps(tps), 
      tick(0.0)
{
}

EntityControl::~EntityControl()
{
}

} // namespace hyperion
