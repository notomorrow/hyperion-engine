#ifndef RENDERER_ACCELERATION_STRUCTURE_H
#define RENDERER_ACCELERATION_STRUCTURE_H

#include "../renderer_result.h"
#include "../renderer_buffer.h"

#include <vector>

#include "renderer_acceleration_structure.h"
#include "renderer_acceleration_structure.h"

namespace hyperion {
namespace renderer {

class Device;
class AccelerationStructure;

enum class AccelerationStructureType {
    BOTTOM_LEVEL,
    TOP_LEVEL
};

enum AccelerationStructureFlags {
    ACCELERATION_STRUCTURE_FLAGS_NONE = 0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING = 1
};

class AccelerationGeometry {
    friend class AccelerationStructure;
    friend class TopLevelAccelerationStructure;
    friend class BottomLevelAccelerationStructure;

public:
    AccelerationGeometry();

    AccelerationGeometry(
        VertexBuffer *vertex_buffer,
        size_t num_vertices,
        size_t vertex_stride,
        IndexBuffer *index_buffer,
        size_t num_indices
    );

    AccelerationGeometry(const AccelerationGeometry &other) = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other) = delete;
    ~AccelerationGeometry();

    void SetVertices(
        VertexBuffer *vertex_buffer,
        size_t num_vertices,
        size_t vertex_stride)
    {
        m_vertex_buffer = vertex_buffer;
        m_num_vertices  = num_vertices;
        m_vertex_stride = vertex_stride;
    }

    void SetIndices(
        IndexBuffer *index_buffer,
        size_t num_indices)
    {
        m_index_buffer = index_buffer;
        m_num_indices  = num_indices;
    }

    Result Create(Device *device);
    /* Remove from the parent acceleration structure */
    Result Destroy(Device *device);

private:
    VertexBuffer                      *m_vertex_buffer;
    size_t                             m_num_vertices;
    size_t                             m_vertex_stride;
    IndexBuffer                       *m_index_buffer;
    size_t                             m_num_indices;
    VkAccelerationStructureGeometryKHR m_geometry;

    AccelerationStructure             *m_acceleration_structure;
};

class AccelerationStructure {
public:
    AccelerationStructure();
    AccelerationStructure(const AccelerationStructure &other) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other) = delete;
    ~AccelerationStructure();

    inline AccelerationStructureBuffer *GetBuffer() const                     { return m_buffer.get(); }
    inline VkAccelerationStructureKHR &GetAccelerationStructure()             { return m_acceleration_structure; }
    inline const VkAccelerationStructureKHR &GetAccelerationStructure() const { return m_acceleration_structure; }

    inline uint64_t GetDeviceAddress() const                                  { return m_device_address; }

    inline AccelerationStructureFlags GetFlags() const                        { return m_flags; }
    inline void SetFlags(AccelerationStructureFlags flags)                    { m_flags = flags; }

    inline void AddGeometry(AccelerationGeometry *geometry)
    {
        AssertThrow(geometry->m_acceleration_structure == nullptr);
        geometry->m_acceleration_structure = this;

        m_geometries.push_back(geometry);
    }

    inline const std::vector<AccelerationGeometry *> &GetGeometries() const
        { return m_geometries; }

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    void RemoveGeometry(AccelerationGeometry *geometry);

    Result Destroy(Instance *instance);

protected:
    static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType);

    Result CreateAccelerationStructure(Instance *instance,
        AccelerationStructureType type,
        const VkAccelerationStructureBuildSizesInfoKHR &build_sizes_info,
        std::vector<VkAccelerationStructureGeometryKHR> &&geometries,
        std::vector<uint32_t> &&primitive_counts);
    
    std::unique_ptr<AccelerationStructureBuffer>          m_buffer;
    std::unique_ptr<AccelerationStructureInstancesBuffer> m_instances_buffer;
    std::vector<AccelerationGeometry *>                   m_geometries;
    VkAccelerationStructureKHR                            m_acceleration_structure;
    uint64_t                                              m_device_address;
    AccelerationStructureFlags                            m_flags;
};

class TopLevelAccelerationStructure : public AccelerationStructure {
public:
    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other) = delete;
    TopLevelAccelerationStructure &operator=(const TopLevelAccelerationStructure &other) = delete;
    ~TopLevelAccelerationStructure();

    inline AccelerationStructureType GetType() const { return AccelerationStructureType::TOP_LEVEL; }
    
    Result Create(Instance *instance, AccelerationStructure *bottom_level);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance, AccelerationStructure *bottom_level);

private:
    Result Rebuild(Instance *instance, AccelerationStructure *bottom_level);
};

class BottomLevelAccelerationStructure : public AccelerationStructure {
public:
    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other) = delete;
    BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &other) = delete;
    ~BottomLevelAccelerationStructure();

    inline AccelerationStructureType GetType() const { return AccelerationStructureType::BOTTOM_LEVEL; }
    
    Result Create(Instance *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance);

private:
    Result Rebuild(Instance *instance);
};

} // namespace renderer
} // namespace hyperion

#endif
