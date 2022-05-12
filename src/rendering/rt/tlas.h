#ifndef HYPERION_V2_TLAS_H
#define HYPERION_V2_TLAS_H

#include <rendering/base.h>
#include "blas.h"

#include <rendering/backend/rt/renderer_acceleration_structure.h>

namespace hyperion::v2 {

using renderer::TopLevelAccelerationStructure;

class Tlas : public EngineComponent<TopLevelAccelerationStructure> {
public:
    Tlas();
    Tlas(const Tlas &other) = delete;
    Tlas &operator=(const Tlas &other) = delete;
    ~Tlas();

    void AddBlas(Ref<Blas> &&blas);

    void Init(Engine *engine);

private:
    std::vector<Ref<Blas>> m_blas;
};

} // namespace hyperion::v2

#endif