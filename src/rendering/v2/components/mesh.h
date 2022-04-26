#ifndef HYPERION_V2_MESH_H
#define HYPERION_V2_MESH_H

#include "base.h"

#include <math/bounding_box.h>

#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/rt/renderer_acceleration_structure.h>

#include <math/vertex.h>

#include <cstdint>
#include <vector>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;
using renderer::CommandBuffer;
using renderer::Device;
using renderer::VertexBuffer;
using renderer::IndexBuffer;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;

class Mesh : public EngineComponentBase<STUB_CLASS(Mesh)> {
public:
    using Index = uint32_t;

    enum Flags {
        MESH_FLAGS_NONE = 0,
        MESH_FLAGS_HAS_ACCELERATION_GEOMETRY = 1
    };

    static std::pair<std::vector<Vertex>, std::vector<Index>> CalculateIndices(const std::vector<Vertex> &vertices);

    Mesh(const std::vector<Vertex> &vertices,
        const std::vector<Index> &indices,
        Flags flags = MESH_FLAGS_NONE);
    Mesh(const Mesh &other) = delete;
    Mesh &operator=(const Mesh &other) = delete;
    ~Mesh();

    inline VertexBuffer *GetVertexBuffer() const                 { return m_vbo.get(); }
    inline IndexBuffer *GetIndexBuffer() const                   { return m_ibo.get(); }
    inline AccelerationGeometry *GetAccelerationGeometry() const { return m_acceleration_geometry.get(); }
    const MeshInputAttributeSet &GetVertexAttributes() const     { return m_vertex_attributes; }
    Flags GetFlags() const                                       { return m_flags; }
    inline void SetFlags(Flags flags)                            { m_flags = flags; }
    
    void CalculateTangents();
    void CalculateNormals();
    void InvertNormals();

    BoundingBox CalculateAabb() const;

    void Init(Engine *engine);
    void Render(Engine *engine, CommandBuffer *cmd) const;

private:
    void CreateAccelerationGeometry(Engine *engine);
    std::vector<float> CreatePackedBuffer();
    void Upload(Instance *instance);

    std::unique_ptr<VertexBuffer> m_vbo;
    std::unique_ptr<IndexBuffer>  m_ibo;
    std::unique_ptr<AccelerationGeometry> m_acceleration_geometry;

    MeshInputAttributeSet m_vertex_attributes;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
    
    Flags m_flags;
};

}

#endif //HYPERION_MESH_H
