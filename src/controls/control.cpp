#include "control.h"

namespace hyperion {

Control::Control(const fbom::FBOMType &loadable_type, const double tps)
    : fbom::FBOMLoadable(fbom::FBOMObjectType("CONTROL").Extend(loadable_type)),
      tps(tps),
      tick(0.0),
      m_first_run(true)
{
}

void Control::OnFirstRun(double dt)
{
    // no-op
}

std::shared_ptr<Loadable> Control::Clone()
{
    return CloneImpl();
}

} // namespace hyperion