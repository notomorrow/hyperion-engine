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
    using Index          = uint32_t;

    void CalculateIndices();
    std::vector<float> CreatePackedBuffer();
    void UploadToDevice(Device *device, CommandBuffer *cmd);

public:
    Mesh();
    ~Mesh();

    void SetVertices(const std::vector<Vertex> &vertices);
    void SetVertices(const std::vector<Vertex> &vertices, const std::vector<Index> &indices);
    void CalculateTangents();
    void CalculateNormals();
    void InvertNormals();

    void Create(Device *device, CommandBuffer *cmd);
    void Destroy(Device *device);
    void Render(Device *device, CommandBuffer *cmd) const;

private:
    std::unique_ptr<VertexBuffer> m_vbo;
    std::unique_ptr<IndexBuffer>  m_ibo;

    MeshInputAttributeSet m_vertex_attributes;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
};

}

#endif //HYPERION_MESH_H
