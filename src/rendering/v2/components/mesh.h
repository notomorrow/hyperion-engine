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

class Mesh {
    using Index          = uint32_t;
    using AttributeFlags = uint16_t;

    enum Attribute : AttributeFlags {
        POSITIONS   = 0x01,
        NORMALS     = 0x02,
        TEXCOORDS0  = 0x04,
        TEXCOORDS1  = 0x08,
        TANGENTS    = 0x10,
        BITANGENTS  = 0x20,
        BONEWEIGHTS = 0x40,
        BONEINDICES = 0x80,
    };

    void CalculateIndices();
    std::vector<float> CreatePackedBuffer();
    void UploadToDevice(renderer::CommandBuffer *cmd);
public:
    static uint16_t GetVertexSize(AttributeFlags vertex_attrs);

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

    AttributeFlags m_vertex_attributes = ( POSITIONS | NORMALS | TEXCOORDS0 | TANGENTS  | BITANGENTS );

    renderer::Instance *m_renderer;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
};

}

#endif //HYPERION_MESH_H
