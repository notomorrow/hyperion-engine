//
// Created by emd22 on 2022-03-19.
//

#include "mesh.h"

#include "../engine.h"

#include <rendering/backend/renderer_command_buffer.h>

#include <vector>
#include <map>
#include <cstring>

namespace hyperion::v2 {

Mesh::Mesh(EngineCallbacks &callbacks)
    : EngineComponentBase(callbacks),
      m_vertex_attributes(
          MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT)
{
}

Mesh::~Mesh()
{
    AssertThrowMsg(m_vbo == nullptr, "Expected vbo to be destroyed before destructor call");
    AssertThrowMsg(m_ibo == nullptr, "Expected vbo to be destroyed before destructor call");
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                   \
    {                                                                           \
        memcpy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float));\
        current_offset += (arg_size);                                           \
    }

std::vector<float> Mesh::CreatePackedBuffer() {
    const size_t vertex_size = m_vertex_attributes.VertexSize();

    std::vector<float> packed_buffer(vertex_size * m_vertices.size());

    /* Raw buffer that is used with our helper macro. */
    float *raw_buffer = packed_buffer.data();
    size_t current_offset = 0;

    for (size_t i = 0; i < m_vertices.size(); i++) {
        auto &vertex = m_vertices[i];
        /* Offset aligned to the current vertex */
        //current_offset = i * vertex_size;

        /* Position and normals */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION)  PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL)    PACKED_SET_ATTR(vertex.GetNormal().values,   3);
        /* Texture coordinates */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0) PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1) PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2)
        /* Tangents and Bitangents */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)   PACKED_SET_ATTR(vertex.GetTangent().values,   3);
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT) PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES) {
            float indices[4] = {
                    (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                    (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, std::size(indices));
        }
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS) {
            float weights[4] = {
                    (float)vertex.GetBoneWeight(0), (float)vertex.GetBoneWeight(1),
                    (float)vertex.GetBoneWeight(2), (float)vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }
    }
    return packed_buffer;
}

#undef PACKED_SET_ATTR

void Mesh::UploadToDevice(Device *device)
{
    AssertThrow(m_vbo == nullptr);
    AssertThrow(m_ibo == nullptr);
    
    m_vbo = std::make_unique<VertexBuffer>();
    m_ibo = std::make_unique<IndexBuffer>();

    std::vector<float> packed_buffer = CreatePackedBuffer();

    /* Create and upload the VBO */
    const size_t packed_buffer_size = packed_buffer.size() * sizeof(float);
    m_vbo->Create(device, packed_buffer_size);
    m_vbo->Copy(device, packed_buffer_size, packed_buffer.data());

    /* Create and upload the indices */
    const size_t packed_indices_size = m_indices.size() * sizeof(Index);
    m_ibo->Create(device, packed_indices_size);
    m_ibo->Copy(device, packed_indices_size, m_indices.data());
}

void Mesh::CalculateIndices()
{
    m_indices.clear();
    std::map<Vertex, Index> index_map;

    /* This will be our resulting buffer with only the vertices we need. */
    std::vector<Vertex> new_vertices;
    new_vertices.reserve(m_vertices.size());

    for (const auto &vertex : m_vertices) {
        /* Check if the vertex already exists in our map */
        auto it = index_map.find(vertex);

        /* If it does, push to our indices */
        if (it != index_map.end()) {
            m_indices.push_back(it->second);

            continue;
        }

        Index mesh_index = static_cast<Index>(new_vertices.size());

        /* The vertex is unique, so we push it. */
        index_map[vertex] = mesh_index;
        new_vertices.push_back(vertex);

        m_indices.push_back(mesh_index);
    }

    m_vertices = new_vertices;
}

void Mesh::SetVertices(const std::vector<Vertex> &vertices)
{
    m_vertices = vertices;

    CalculateIndices();
}

void Mesh::SetVertices(const std::vector<Vertex> &vertices, const std::vector<Index> &indices)
{
    m_vertices = vertices;
    m_indices  = indices;
}

void Mesh::Create(Engine *engine)
{
    Track(
        engine->callbacks.Once(EngineCallback::CREATE_MESHES, [this, engine](...) {
            UploadToDevice(engine->GetInstance()->GetDevice());
        }),
        engine->callbacks.Once(EngineCallback::DESTROY_MESHES, [this, engine](...) {
            Destroy(engine);
        })
    );
}

void Mesh::Destroy(Engine *engine)
{
    AssertThrow(m_vbo != nullptr);
    AssertThrow(m_ibo != nullptr);

    Device *device = engine->GetInstance()->GetDevice();

    m_vbo->Destroy(device);
    m_vbo.reset();

    m_ibo->Destroy(device);
    m_ibo.reset();
}

void Mesh::Render(Engine *engine, CommandBuffer *cmd) const
{
    m_vbo->Bind(cmd);
    m_ibo->Bind(cmd);

    vkCmdDrawIndexed(
        cmd->GetCommandBuffer(),
        uint32_t(m_indices.size()),
        1,
        0,
        0,
        0
    );
}

void Mesh::CalculateNormals()
{
    if (m_indices.empty()) {
        DebugLog(LogType::Warn, "Cannot calculate normals before indices are generated!\n");

        return;
    }

    std::unordered_map<MeshIndex, std::vector<Vector3>> normals;
    //std::vector<Vector3> normals(m_indices.size());

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        Index i0 = m_indices[i];
        Index i1 = m_indices[i + 1];
        Index i2 = m_indices[i + 2];

        const Vector3 &p0 = m_vertices[i0].GetPosition();
        const Vector3 &p1 = m_vertices[i1].GetPosition();
        const Vector3 &p2 = m_vertices[i2].GetPosition();

        Vector3 u = p2 - p0;
        Vector3 v = p1 - p0;
        Vector3 n = v;

        n.Cross(u);
        n.Normalize();

        normals[i0].push_back(n);
        normals[i1].push_back(n);
        normals[i2].push_back(n);
    }

    for (size_t i = 0; i < m_vertices.size(); i++) {
        // find average
        Vector3 average;

        for (const auto &normal : normals[i]) {
            average += normal * (1.0f / float(normals[i].size()));
        }

        average.Normalize();

        m_vertices[i].SetNormal(average);
    }
}

void Mesh::CalculateTangents()
{
    Vertex *v[3];
    Vector2 uv[3];

    for (auto &vertex : m_vertices) {
        vertex.SetTangent(Vector3(0.0f));
        vertex.SetBitangent(Vector3(0.0f));
    }

    std::vector<Vector3> new_tangents(m_vertices.size(), Vector3());
    std::vector<Vector3> new_bitangents(m_vertices.size(), Vector3());

    for (size_t i = 0; i < m_indices.size(); i += 3) {
        for (int j = 0; j < 3; j++) {
            v[j] = &m_vertices[m_indices[i + j]];
            uv[j] = v[j]->GetTexCoord0();
        }

        Vector3 edge1 = v[1]->GetPosition() - v[0]->GetPosition();
        Vector3 edge2 = v[2]->GetPosition() - v[0]->GetPosition();

        Vector2 edge1uv = uv[1] - uv[0];
        Vector2 edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;
        const float mul = 1.0f / cp;

        Vector3 tangent = ((edge1 * edge2uv.y) - (edge2 * edge1uv.y)) * mul;
        Vector3 bitangent = ((edge1 * edge2uv.x) - (edge2 * edge1uv.x)) * mul;

        new_tangents[m_indices[i]] += tangent;
        new_tangents[m_indices[i + 1]] += tangent;
        new_tangents[m_indices[i + 2]] += tangent;

        new_bitangents[m_indices[i]] += bitangent;
        new_bitangents[m_indices[i + 1]] += bitangent;
        new_bitangents[m_indices[i + 2]] += bitangent;
    }

    for (size_t i = 0; i < m_vertices.size(); i++) {
        Vector3 n = m_vertices[i].GetNormal();
        Vector3 tangent = (new_tangents[i] - (n * n.Dot(new_tangents[i])));
        Vector3 cross = n.Cross(new_tangents[i]);

        Vector3 bitangent = cross * MathUtil::Sign(cross.Dot(new_bitangents[i]));

        m_vertices[i].SetTangent(tangent);
        m_vertices[i].SetBitangent(bitangent);
    }
}

void Mesh::InvertNormals()
{
    for (Vertex &vertex : m_vertices) {
        vertex.SetNormal(vertex.GetNormal() * -1.0f);
    }
}


}