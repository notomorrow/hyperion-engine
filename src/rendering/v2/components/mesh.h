//
// Created by emd22 on 2022-03-19.
//

#ifndef HYPERION_MESH_H
#define HYPERION_MESH_H

#include "base.h"

#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_command_buffer.h>

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

class Mesh : public EngineComponentBase<STUB_CLASS(Mesh)> {
public:
    using Index = uint32_t;

    Mesh(const std::vector<Vertex> &vertices,
        const std::vector<Index> &indices);
    ~Mesh();

    inline const MeshInputAttributeSet &GetVertexAttributes() const
        { return m_vertex_attributes; }
    
    void CalculateTangents();
    void CalculateNormals();
    void InvertNormals();

    void Init(Engine *engine);
    void Render(Engine *engine, CommandBuffer *cmd) const;

    static std::pair<std::vector<Vertex>, std::vector<Index>> CalculateIndices(const std::vector<Vertex> &vertices);

private:
    std::vector<float> CreatePackedBuffer();
    void UploadToDevice(Device *device);

    std::unique_ptr<VertexBuffer> m_vbo;
    std::unique_ptr<IndexBuffer>  m_ibo;

    MeshInputAttributeSet m_vertex_attributes;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
};

}

#endif //HYPERION_MESH_H
