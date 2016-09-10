#include "terrain_chunk.h"

namespace apex {
TerrainChunk::TerrainChunk(const ChunkInfo &chunk_info)
    : m_chunk_info(chunk_info)
{
}

std::shared_ptr<Mesh> TerrainChunk::BuildMesh(const std::vector<double> &heights)
{
    std::vector<Vertex> vertices = BuildVertices(heights);
    std::vector<size_t> indices = BuildIndices();
    CalculateNormals(vertices, indices);

    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(vertices, indices);
    mesh->SetAttribute(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute::TexCoords0);
    mesh->SetAttribute(Mesh::ATTR_NORMALS, Mesh::MeshAttribute::Normals);

    return mesh;
}

void TerrainChunk::AddNormal(Vertex &vertex, const Vector3 &normal)
{
    Vector3 before = vertex.GetNormal();
    vertex.SetNormal(before + normal);
}

void TerrainChunk::NormalizeNormal(Vertex &vertex)
{
    Vector3 normal = vertex.GetNormal();
    normal.Normalize();
    vertex.SetNormal(normal);
}

void TerrainChunk::CalculateNormals(std::vector<Vertex> &vertices, const std::vector<size_t> &indices)
{
    for (size_t i = 0; i < indices.size(); i += 3) {
        auto i0 = indices[i];
        auto i1 = indices[i + 1];
        auto i2 = indices[i + 2];

        auto p0 = vertices[i0].GetPosition();
        auto p1 = vertices[i1].GetPosition();
        auto p2 = vertices[i2].GetPosition();

        auto u = p2 - p0;
        auto v = p1 - p0;
        auto n = u;
        n.Cross(v);
        n.Normalize();

        AddNormal(vertices[i0], n);
        AddNormal(vertices[i1], n);
        AddNormal(vertices[i2], n);
    }

    for (auto &&vert : vertices) {
        NormalizeNormal(vert);
    }
}

std::vector<Vertex> TerrainChunk::BuildVertices(const std::vector<double> &heights)
{
    std::vector<Vertex> vertices;
    vertices.resize(m_chunk_info.m_width * m_chunk_info.m_length);

    int i = 0;
    for (int z = 0; z < m_chunk_info.m_length; z++) {
        for (int x = 0; x < m_chunk_info.m_width; x++) {
            Vector3 position(x - m_chunk_info.m_width / 2, heights[i], z - m_chunk_info.m_length / 2);
            position *= m_chunk_info.m_scale;

            Vector2 texcoord(-x / float(m_chunk_info.m_width),
                -z / float(m_chunk_info.m_length));

            vertices[i++] = Vertex(position, texcoord);
        }
    }
    return vertices;
}

std::vector<size_t> TerrainChunk::BuildIndices()
{
    std::vector<size_t> indices;
    indices.resize(6 * (m_chunk_info.m_width - 1) * (m_chunk_info.m_length - 1));

    int pitch = m_chunk_info.m_width;
    int row = 0;

    int i0 = row;
    int i1 = row + 1;
    int i2 = pitch + i1;
    int i3 = pitch + row;

    int i = 0;
    for (int z = 0; z < m_chunk_info.m_length - 1; z++) {
        for (int x = 0; x < m_chunk_info.m_width - 1; x++) {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i1;
            indices[i++] = i2;
            indices[i++] = i0;
            indices[i++] = i3;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}
}