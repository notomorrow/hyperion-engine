#ifndef HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H
#define HYPERION_V2_ACCELERATION_STRUCTURE_BUILDER_H

#include "../components/spatial.h"

#include <rendering/backend/rt/renderer_acceleration_structure.h>

#include <memory>

namespace hyperion::v2 {

using renderer::BottomLevelAccelerationStructure;
using renderer::AccelerationGeometry;

class Engine;

class AccelerationStructureBuilder {
public:
    AccelerationStructureBuilder(const Spatial *spatial);
    AccelerationStructureBuilder(const AccelerationStructureBuilder &other) = delete;
    AccelerationStructureBuilder &operator=(const AccelerationStructureBuilder &other) = delete;
    ~AccelerationStructureBuilder() = default;

    std::unique_ptr<BottomLevelAccelerationStructure> Build(Engine *engine);

private:
    void CreateGeometry(Engine *engine);
    void DestroyGeometry(Engine *engine);

    const Spatial *m_spatial;
    std::unique_ptr<AccelerationGeometry> m_geometry;
};

} // namespace hyperion::v2

#endif