#ifndef HYPERION_V2_PROBE_SYSTEM_H
#define HYPERION_V2_PROBE_SYSTEM_H

#include <math/bounding_box.h>

namespace hyperion::v2 {

struct ProbeSystemSetup {
    BoundingBox aabb;
};

class ProbeSystem {
public:
    ProbeSystem();
    ProbeSystem(const ProbeSystem &other) = delete;
    ProbeSystem &operator=(const ProbeSystem &other) = delete;
    ~ProbeSystem();
};

} // namespace hyperion::v2

#endif