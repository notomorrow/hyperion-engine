#include "control.h"

namespace hyperion {

EntityControl::EntityControl(const double tps)
    : tps(tps), 
      tick(0.0),
      m_first_run(true)
{
}

EntityControl::~EntityControl()
{
}

void EntityControl::OnFirstRun(double dt)
{
    // no-op
}

} // namespace hyperion
