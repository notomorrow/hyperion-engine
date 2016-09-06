#include "mesh.h"
#include "../core_engine.h"

namespace apex {

const Mesh::MeshAttribute Mesh::MeshAttribute::Positions = {0, 3, 0};
const Mesh::MeshAttribute Mesh::MeshAttribute::Normals = { 0, 3, 1 };
const Mesh::MeshAttribute Mesh::MeshAttribute::TexCoords0 = { 0, 2, 2 };
const Mesh::MeshAttribute Mesh::MeshAttribute::TexCoords1 = { 0, 2, 3 };
const Mesh::MeshAttribute Mesh::MeshAttribute::Tangents = { 0, 3, 4 };
const Mesh::MeshAttribute Mesh::MeshAttribute::Bitangents = { 0, 3, 5 };
const Mesh::MeshAttribute Mesh::MeshAttribute::BoneWeights = { 0, 4, 6 };
const Mesh::MeshAttribute Mesh::MeshAttribute::BoneIndices = { 0, 4, 7 };

Mesh::Mesh()
{
    SetAttribute(ATTR_POSITIONS, MeshAttribute::Positions);
    SetPrimitiveType(PRIM_TRIANGLES);
    is_uploaded = false;
    is_created = false;
}

Mesh::~Mesh()
{
    if (is_created) {
        CoreEngine::GetInstance()->DeleteBuffers(1, &vbo);
        CoreEngine::GetInstance()->DeleteBuffers(1, &ibo);
    }

    is_uploaded = false;
    is_created = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts)
{
    vertices = verts;
    indices.clear();
    for (size_t i = 0; i < verts.size(); i++) {
        indices.push_back(i);
    }
    is_uploaded = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts, const std::vector<uint32_t> &ind)
{
    vertices = verts;
    indices = ind;
    is_uploaded = false;
}

std::vector<Vertex> Mesh::GetVertices() const
{
    return vertices;
}

std::vector<uint32_t> Mesh::GetIndices() const
{
    return indices;
}

void Mesh::SetAttribute(MeshAttributeType type, const MeshAttribute &attr)
{
    attribs[type] = attr;
    is_uploaded = false;
}

void Mesh::SetPrimitiveType(Mesh::PrimitiveType type)
{
    primitive_type = type;
}

Mesh::PrimitiveType Mesh::GetPrimitiveType() const
{
    return primitive_type;
}

std::vector<float> Mesh::CreateBuffer()
{
    unsigned int vert_size = 0, prev_size = 0, offset = 0;

    for (auto &&attr : attribs) {
        offset += prev_size;
        attr.second.offset = offset;
        prev_size = attr.second.size;
        vert_size += prev_size;
    }

    vertex_size = vert_size;

    std::vector<float> buffer(vert_size * vertices.size());

    auto pos_it = attribs.find(ATTR_POSITIONS);
    auto norm_it = attribs.find(ATTR_NORMALS);
    auto tc0_it = attribs.find(ATTR_TEXCOORDS0);
    auto tc1_it = attribs.find(ATTR_TEXCOORDS1);
    auto tan_it = attribs.find(ATTR_TANGENTS);
    auto bitan_it = attribs.find(ATTR_BITANGENTS);
    auto bonew_it = attribs.find(ATTR_BONEWEIGHTS);
    auto bonei_it = attribs.find(ATTR_BONEINDICES);

    for (size_t i = 0; i < vertices.size(); i++) {
        auto &vertex = vertices[i];
        if (pos_it != attribs.end()) {
            buffer[(i * vert_size) + pos_it->second.offset] = vertex.GetPosition().x;
            buffer[(i * vert_size) + pos_it->second.offset + 1] = vertex.GetPosition().y;
            buffer[(i * vert_size) + pos_it->second.offset + 2] = vertex.GetPosition().z;
        }
        if (norm_it != attribs.end()) {
            buffer[(i * vert_size) + norm_it->second.offset] = vertex.GetNormal().x;
            buffer[(i * vert_size) + norm_it->second.offset + 1] = vertex.GetNormal().y;
            buffer[(i * vert_size) + norm_it->second.offset + 2] = vertex.GetNormal().z;
        }
        if (tc0_it != attribs.end()) {
            buffer[(i * vert_size) + tc0_it->second.offset] = vertex.GetTexCoord0().x;
            buffer[(i * vert_size) + tc0_it->second.offset + 1] = vertex.GetTexCoord0().y;
        }
        if (tc1_it != attribs.end()) {
            buffer[(i * vert_size) + tc1_it->second.offset] = vertex.GetTexCoord1().x;
            buffer[(i * vert_size) + tc1_it->second.offset + 1] = vertex.GetTexCoord1().y;
        }
        if (tan_it != attribs.end()) {
            buffer[(i * vert_size) + tan_it->second.offset] = vertex.GetTangent().x;
            buffer[(i * vert_size) + tan_it->second.offset + 1] = vertex.GetTangent().y;
            buffer[(i * vert_size) + tan_it->second.offset + 2] = vertex.GetTangent().z;
        }
        if (bitan_it != attribs.end()) {
            buffer[(i * vert_size) + bitan_it->second.offset] = vertex.GetBitangent().x;
            buffer[(i * vert_size) + bitan_it->second.offset + 1] = vertex.GetBitangent().y;
            buffer[(i * vert_size) + bitan_it->second.offset + 2] = vertex.GetBitangent().z;
        }
        if (bonei_it != attribs.end()) {
            buffer[(i * vert_size) + bonei_it->second.offset] = static_cast<float>(vertex.GetBoneIndex(0));
            buffer[(i * vert_size) + bonei_it->second.offset + 1] = static_cast<float>(vertex.GetBoneIndex(1));
            buffer[(i * vert_size) + bonei_it->second.offset + 2] = static_cast<float>(vertex.GetBoneIndex(2));
            buffer[(i * vert_size) + bonei_it->second.offset + 3] = static_cast<float>(vertex.GetBoneIndex(3));
        }
        if (bonew_it != attribs.end()) {
            buffer[(i * vert_size) + bonew_it->second.offset] = vertex.GetBoneWeight(0);
            buffer[(i * vert_size) + bonew_it->second.offset + 1] = vertex.GetBoneWeight(1);
            buffer[(i * vert_size) + bonew_it->second.offset + 2] = vertex.GetBoneWeight(2);
            buffer[(i * vert_size) + bonew_it->second.offset + 3] = vertex.GetBoneWeight(3);
        }
    }

    return buffer;
}

void Mesh::Render()
{
    auto *engine = CoreEngine::GetInstance();

    if (!is_created) {
        engine->GenBuffers(1, &vbo);
        engine->GenBuffers(1, &ibo);
        is_created = true;
    }
    if (!is_uploaded) {
        std::vector<float> buffer = CreateBuffer();
        engine->BindBuffer(CoreEngine::ARRAY_BUFFER, vbo);
        engine->BufferData(CoreEngine::ARRAY_BUFFER, buffer.size() * sizeof(float), &buffer[0], CoreEngine::STATIC_DRAW);
        engine->BindBuffer(CoreEngine::ELEMENT_ARRAY_BUFFER, ibo);
        engine->BufferData(CoreEngine::ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], CoreEngine::STATIC_DRAW);
        engine->BindBuffer(CoreEngine::ARRAY_BUFFER, 0);
        engine->BindBuffer(CoreEngine::ELEMENT_ARRAY_BUFFER, 0);
        is_uploaded = true;
    }

    engine->BindBuffer(CoreEngine::ARRAY_BUFFER, vbo);

    for (auto &&attr : attribs) {
        engine->EnableVertexAttribArray(attr.second.index);
        engine->VertexAttribPointer(attr.second.index, attr.second.size, CoreEngine::FLOAT,
            false, vertex_size * sizeof(float), (void*)(attr.second.offset * sizeof(float)));
    }

    engine->BindBuffer(CoreEngine::ELEMENT_ARRAY_BUFFER, ibo);
    engine->DrawElements(primitive_type, indices.size(), CoreEngine::UNSIGNED_INT, 0);

    for (auto &&attr : attribs) {
        engine->DisableVertexAttribArray(attr.second.index);
    }

    // Unbind the buffers
    engine->BindBuffer(CoreEngine::ARRAY_BUFFER, 0);
    engine->BindBuffer(CoreEngine::ELEMENT_ARRAY_BUFFER, 0);
}
}