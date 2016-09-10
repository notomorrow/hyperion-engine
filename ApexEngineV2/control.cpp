#include "control.h"

namespace apex {
EntityControl::EntityControl(const double tps)
    : tps(tps), tick(0)
{
}

EntityControl::~EntityControl()
{
}
} // namespace apex