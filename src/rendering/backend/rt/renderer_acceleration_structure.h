#ifndef RENDERER_ACCELERATION_STRUCTURE_H
#define RENDERER_ACCELERATION_STRUCTURE_H

#include "../renderer_result.h"
#include "../renderer_buffer.h"

#include <vector>

namespace hyperion {
namespace renderer {

class Device;

enum class AccelerationStructureType {
    BOTTOM_LEVEL,
    TOP_LEVEL
};

class AccelerationGeometry {
    friend class AccelerationStructure;
public:
    AccelerationGeometry(VertexBuffer *vertex_buffer,
        size_t num_vertices, size_t vertex_stride,
        IndexBuffer *index_buffer,
        size_t num_indices);
    AccelerationGeometry(const AccelerationGeometry &other) = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other) = delete;
    ~AccelerationGeometry();

    Result Create(Device *device);

private:
    AccelerationStructureType m_type;
    VertexBuffer *m_vertex_buffer;
    size_t m_num_vertices;
    size_t m_vertex_stride;
    IndexBuffer *m_index_buffer;
    size_t m_num_indices;
    VkAccelerationStructureGeometryKHR m_geometry;
};

class AccelerationStructure {
public:
    AccelerationStructure(AccelerationStructureType type);
    AccelerationStructure(const AccelerationStructure &other) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other) = delete;
    ~AccelerationStructure();

    void AddGeometry(std::unique_ptr<AccelerationGeometry> &&geometry)
        { m_geometries.push_back(std::move(geometry)); }

    Result Create(Instance *instance);
    Result Destroy(Device *device);

private:
    AccelerationStructureType m_type;
    std::unique_ptr<AccelerationStructureBuffer> m_buffer;
    std::vector<std::unique_ptr<AccelerationGeometry>> m_geometries;
    VkAccelerationStructureKHR m_acceleration_structure;
    uint64_t m_device_address;
};

} // namespace renderer
} // namespace hyperion

#endif