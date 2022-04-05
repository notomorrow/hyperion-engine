//
// Created by emd22 on 2022-03-19.
//

#ifndef HYPERION_MESH_H
#define HYPERION_MESH_H

#include <cstdint>
#include <vector>

#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_command_buffer.h>
#include <math/vertex.h>

namespace hyperion::v2 {

using renderer::MeshInputAttribute;
using renderer::MeshInputAttributeSet;

class Mesh {
    using Index          = uint32_t;

    void CalculateIndices();
    std::vector<float> CreatePackedBuffer();
    void UploadToDevice(renderer::CommandBuffer *cmd);
public:
    Mesh(renderer::Instance *renderer_instance);
    inline void SetVertices(const std::vector<Vertex> &vertices);
    inline void SetVertices(const std::vector<Vertex> &vertices, const std::vector<Index> &indices);
    void CalculateTangents();
    void CalculateNormals();
    void InvertNormals();

    void Create(renderer::CommandBuffer *cmd);
    inline void Draw(renderer::CommandBuffer *cmd);
    void Destroy();

    ~Mesh();
private:
    std::unique_ptr<renderer::VertexBuffer> m_vbo = nullptr;
    std::unique_ptr<renderer::IndexBuffer>  m_ibo = nullptr;

    MeshInputAttributeSet m_vertex_attributes;

    renderer::Instance *m_renderer;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
};

}

#endif //HYPERION_MESH_H
