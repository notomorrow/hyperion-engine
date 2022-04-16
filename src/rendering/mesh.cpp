#include "mesh.h"
#include "../gl_util.h"

#include "renderer.h"

namespace hyperion {

static const EnumOptions<Mesh::MeshAttributeType, Mesh::MeshAttribute, 8> attributes({
    std::make_pair(Mesh::ATTR_POSITIONS, Mesh::MeshAttribute({ 0, 3, 0 })),
    std::make_pair(Mesh::ATTR_NORMALS, Mesh::MeshAttribute({ 0, 3, 1 })),
    std::make_pair(Mesh::ATTR_TEXCOORDS0, Mesh::MeshAttribute({ 0, 2, 2 })),
    std::make_pair(Mesh::ATTR_TEXCOORDS1, Mesh::MeshAttribute({ 0, 2, 3 })),
    std::make_pair(Mesh::ATTR_TANGENTS, Mesh::MeshAttribute({ 0, 3, 4 })),
    std::make_pair(Mesh::ATTR_BITANGENTS, Mesh::MeshAttribute({ 0, 3, 5 })),
    std::make_pair(Mesh::ATTR_BONEWEIGHTS, Mesh::MeshAttribute({ 0, 4, 6 })),
    std::make_pair(Mesh::ATTR_BONEINDICES, Mesh::MeshAttribute({0, 4, 7})),
});

Mesh::Mesh()
    : Renderable(fbom::FBOMObjectType("MESH")),
      _render_context(nullptr)
{
    EnableAttribute(ATTR_POSITIONS);
    EnableAttribute(ATTR_NORMALS);
    EnableAttribute(ATTR_TEXCOORDS0);
    EnableAttribute(ATTR_TEXCOORDS1);
    EnableAttribute(ATTR_TANGENTS);
    EnableAttribute(ATTR_BITANGENTS);
    SetPrimitiveType(PRIM_TRIANGLES);
    is_uploaded = false;
    is_created = false;
}

Mesh::~Mesh()
{
    std::cout << "Destroy old mesh \n";
    if (_render_context != nullptr) {
        delete _render_context;
    }

    is_uploaded = false;
    is_created = false;
}

void Mesh::EnableAttribute(MeshAttributeType type)
{
    auto it = attribs.find(type);

    if (it != attribs.end()) {
        return;
    }

    attribs.emplace(type, attributes.Get(type));

    is_uploaded = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts)
{
    vertices = verts;
    indices.clear();
    for (size_t i = 0; i < verts.size(); i++) {
        indices.push_back(static_cast<MeshIndex>(i));
    }

    // update the aabb
    // TODO: more concrete (virtual method?) way of setting aabb on Renderable
    m_aabb.Clear();
    for (Vertex &vertex : vertices) {
        m_aabb.Extend(vertex.GetPosition());
    }

    is_uploaded = false;
}

void Mesh::SetVertices(const std::vector<Vertex> &verts, const std::vector<MeshIndex> &ind)
{
    vertices = verts;
    indices = ind;

    // update the aabb
    m_aabb.Clear();
    for (Vertex &vertex : vertices) {
        m_aabb.Extend(vertex.GetPosition());
    }

    is_uploaded = false;
}

void Mesh::SetTriangles(const std::vector<Triangle> &triangles)
{
    std::vector<Vertex> verts;
    verts.resize(triangles.size() * 3);

    for (size_t i = 0; i < triangles.size(); i++) {
        for (int j = 0; j < 3; j++) {
            verts[i * 3 + j] = triangles[i][j];
        }
    }

    SetVertices(verts);
}

std::vector<Triangle> Mesh::CalculateTriangleBuffer() const
{
    // TODO: assert mesh is triangle type?
    AssertThrow(indices.size() % 3 == 0);

    std::vector<Triangle> triangles;
    triangles.reserve(indices.size() * 3);

    for (size_t i = 0; i < indices.size(); i += 3) {
        MeshIndex i0 = indices[i];
        MeshIndex i1 = indices[i + 1];
        MeshIndex i2 = indices[i + 2];

        triangles.emplace_back(Triangle(
            vertices[i0],
            vertices[i1],
            vertices[i2]
        ));
    }

    return triangles;
}

void Mesh::CalculateVertexSize()
{
    unsigned int vert_size = 0, prev_size = 0, offset = 0;

    for (auto &attrib : attribs) {
        offset += prev_size;
        attrib.second.offset = offset;
        prev_size = attrib.second.size;
        vert_size += prev_size;
    }

    vertex_size = vert_size;
}

std::vector<float> Mesh::CreateBuffer()
{
    CalculateVertexSize();

    std::vector<float> buffer(vertex_size * vertices.size());

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
            buffer[(i * vertex_size) + pos_it->second.offset] = vertex.GetPosition().x;
            buffer[(i * vertex_size) + pos_it->second.offset + 1] = vertex.GetPosition().y;
            buffer[(i * vertex_size) + pos_it->second.offset + 2] = vertex.GetPosition().z;
        }
        if (norm_it != attribs.end()) {
            buffer[(i * vertex_size) + norm_it->second.offset] = vertex.GetNormal().x;
            buffer[(i * vertex_size) + norm_it->second.offset + 1] = vertex.GetNormal().y;
            buffer[(i * vertex_size) + norm_it->second.offset + 2] = vertex.GetNormal().z;
        }
        if (tc0_it != attribs.end()) {
            buffer[(i * vertex_size) + tc0_it->second.offset] = vertex.GetTexCoord0().x;
            buffer[(i * vertex_size) + tc0_it->second.offset + 1] = vertex.GetTexCoord0().y;
        }
        if (tc1_it != attribs.end()) {
            buffer[(i * vertex_size) + tc1_it->second.offset] = vertex.GetTexCoord1().x;
            buffer[(i * vertex_size) + tc1_it->second.offset + 1] = vertex.GetTexCoord1().y;
        }
        if (tan_it != attribs.end()) {
            buffer[(i * vertex_size) + tan_it->second.offset] = vertex.GetTangent().x;
            buffer[(i * vertex_size) + tan_it->second.offset + 1] = vertex.GetTangent().y;
            buffer[(i * vertex_size) + tan_it->second.offset + 2] = vertex.GetTangent().z;
        }
        if (bitan_it != attribs.end()) {
            buffer[(i * vertex_size) + bitan_it->second.offset] = vertex.GetBitangent().x;
            buffer[(i * vertex_size) + bitan_it->second.offset + 1] = vertex.GetBitangent().y;
            buffer[(i * vertex_size) + bitan_it->second.offset + 2] = vertex.GetBitangent().z;
        }
        if (bonei_it != attribs.end()) {
            buffer[(i * vertex_size) + bonei_it->second.offset] = static_cast<float>(vertex.GetBoneIndex(0));
            buffer[(i * vertex_size) + bonei_it->second.offset + 1] = static_cast<float>(vertex.GetBoneIndex(1));
            buffer[(i * vertex_size) + bonei_it->second.offset + 2] = static_cast<float>(vertex.GetBoneIndex(2));
            buffer[(i * vertex_size) + bonei_it->second.offset + 3] = static_cast<float>(vertex.GetBoneIndex(3));
        }
        if (bonew_it != attribs.end()) {
            buffer[(i * vertex_size) + bonew_it->second.offset] = vertex.GetBoneWeight(0);
            buffer[(i * vertex_size) + bonew_it->second.offset + 1] = vertex.GetBoneWeight(1);
            buffer[(i * vertex_size) + bonew_it->second.offset + 2] = vertex.GetBoneWeight(2);
            buffer[(i * vertex_size) + bonew_it->second.offset + 3] = vertex.GetBoneWeight(3);
        }
    }

    return buffer;
}

void Mesh::SetVerticesFromFloatBuffer(const std::vector<float> &buffer)
{
    CalculateVertexSize();

    AssertThrowMsg(buffer.size() % vertex_size == 0, "vertex size does not evenly divide into buffer size -- mismatch!");

    size_t num_vertices = buffer.size() / vertex_size;

    std::vector<Vertex> result;
    result.reserve(num_vertices);

    auto pos_it = attribs.find(ATTR_POSITIONS);
    auto norm_it = attribs.find(ATTR_NORMALS);
    auto tc0_it = attribs.find(ATTR_TEXCOORDS0);
    auto tc1_it = attribs.find(ATTR_TEXCOORDS1);
    auto tan_it = attribs.find(ATTR_TANGENTS);
    auto bitan_it = attribs.find(ATTR_BITANGENTS);
    auto bonew_it = attribs.find(ATTR_BONEWEIGHTS);
    auto bonei_it = attribs.find(ATTR_BONEINDICES);

    for (unsigned int i = 0; i < buffer.size(); i += vertex_size) {
        Vertex v;

        if (pos_it != attribs.end()) {
            v.SetPosition(Vector3(
                buffer[i + pos_it->second.offset],
                buffer[i + pos_it->second.offset + 1],
                buffer[i + pos_it->second.offset + 2]
            ));
        }

        if (norm_it != attribs.end()) {
            v.SetNormal(Vector3(
                buffer[i + norm_it->second.offset],
                buffer[i + norm_it->second.offset + 1],
                buffer[i + norm_it->second.offset + 2]
            ));
        }

        if (tc0_it != attribs.end()) {
            v.SetTexCoord0(Vector2(
                buffer[i + tc0_it->second.offset],
                buffer[i + tc0_it->second.offset + 1]
            ));
        }

        if (tc1_it != attribs.end()) {
            v.SetTexCoord1(Vector2(
                buffer[i + tc1_it->second.offset],
                buffer[i + tc1_it->second.offset + 1]
            ));
        }

        if (tan_it != attribs.end()) {
            v.SetTangent(Vector3(
                buffer[i + tan_it->second.offset],
                buffer[i + tan_it->second.offset + 1],
                buffer[i + tan_it->second.offset + 2]
            ));
        }

        if (bitan_it != attribs.end()) {
            v.SetBitangent(Vector3(
                buffer[i + bitan_it->second.offset],
                buffer[i + bitan_it->second.offset + 1],
                buffer[i + bitan_it->second.offset + 2]
            ));
        }

        if (bonei_it != attribs.end()) {
            for (int j = 0; j < 4; j++) {
                v.SetBoneIndex(j, buffer[i + bonei_it->second.offset + j]);
            }
        }

        if (bonew_it != attribs.end()) {
            for (int j = 0; j < 4; j++) {
                v.SetBoneWeight(j, buffer[i + bonew_it->second.offset + j]);
            }
        }

        result.push_back(v);
    }

    SetVertices(result);
}

void Mesh::Render(Renderer *renderer, Camera *cam) {

}

void Mesh::RenderVk(renderer::CommandBuffer *command_buffer, renderer::Instance *vk_renderer, Camera *cam) {
    VkCommandBuffer cmd = command_buffer->GetCommandBuffer();

    if (!is_created) {
        _render_context = new RenderContext(this, vk_renderer);
        _render_context->Create(cmd);
        is_created = true;
    }

    if (!is_uploaded) {
        _render_context->Upload(cmd);
        is_uploaded = true;
    }

    _render_context->Draw(cmd);
}

/*
void Mesh::Render(Renderer *renderer, Camera *cam)
{
    if (!is_created) {
        glGenVertexArrays(1, &vao);
        CatchGLErrors("Failed to generate vertex arrays.");

        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);

        is_created = true;
    }

    glBindVertexArray(vao);

    if (!is_uploaded) {
        std::vector<float> buffer = CreateBuffer();

        if (buffer.empty()) {
            is_uploaded = true;

            AssertSoftMsg(false, "CreateBuffer() returned no data");
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), &buffer[0], GL_STATIC_DRAW);
        CatchGLErrors("Failed to set buffer data.");

        unsigned int error;

        for (auto &&attr : attribs) {
            glEnableVertexAttribArray(attr.second.index);
            CatchGLErrors("Failed to enable vertex attribute array." __FILE__);

            glVertexAttribPointer(attr.second.index, attr.second.size, GL_FLOAT,
                false, vertex_size * sizeof(float), (void*)(attr.second.offset * sizeof(float)));

            CatchGLErrors("Failed to set vertex attribute pointer.");
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(MeshIndex), &indices[0], GL_STATIC_DRAW);

        is_uploaded = true;
    }

    glDrawElements(primitive_type, indices.size(), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
}
*/
bool Mesh::IntersectRay(const Ray &ray, const Transform &transform, RaytestHit &out) const
{
    if (primitive_type != PRIM_TRIANGLES) {
        // fall back to aabb test
        return Renderable::IntersectRay(ray, transform, out);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        Triangle t(
            vertices[indices[i]].GetPosition(),
            vertices[indices[i + 1]].GetPosition(),
            vertices[indices[i + 2]].GetPosition()
        );

        t *= transform;

        if (t.IntersectRay(ray, out)) {
            return true;
        }
    }

    return false;
}

bool Mesh::IntersectRay(const Ray &ray, const Transform &transform, RaytestHitList_t &out) const
{
    bool intersected = false;

    if (primitive_type != PRIM_TRIANGLES) {
        // fall back to aabb test
        return Renderable::IntersectRay(ray, transform, out);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        Triangle t(
            vertices[indices[i]].GetPosition(),
            vertices[indices[i + 1]].GetPosition(),
            vertices[indices[i + 2]].GetPosition()
        );

        t *= transform;

        RaytestHit intersection;

        if (t.IntersectRay(ray, intersection)) {
            intersected = true;

            out.push_back(intersection);
        }
    }

    return intersected;
}

void Mesh::CalculateNormals()
{
    // reset to zero initially
    for (Vertex &vert : vertices) {
        vert.SetNormal(Vector3::Zero());
    }

    std::vector<Vector3> new_normals(indices.size(), Vector3(0.0f));

    std::unordered_map<MeshIndex, std::vector<Vector3>> normals;

    for (size_t i = 0; i < indices.size(); i += 3) {
        MeshIndex i0 = indices[i];
        MeshIndex i1 = indices[i + 1];
        MeshIndex i2 = indices[i + 2];

        const Vector3 &p0 = vertices[i0].GetPosition();
        const Vector3 &p1 = vertices[i1].GetPosition();
        const Vector3 &p2 = vertices[i2].GetPosition();

        Vector3 u = p2 - p0;
        Vector3 v = p1 - p0;
        Vector3 n = v;

        n.Cross(u);
        n.Normalize();

        normals[i0].push_back(n);
        normals[i1].push_back(n);
        normals[i2].push_back(n);
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        // find average
        Vector3 average;

        for (const auto &normal : normals[i]) {
            average += normal * (1.0f / float(normals[i].size()));
        }

        average.Normalize();

        vertices[i].SetNormal(average);
    }

    EnableAttribute(ATTR_NORMALS);

    CalculateTangents();
}

void Mesh::CalculateTangents()
{
    Vertex *v[3];
    Vector2 uv[3];

    for (auto &vertex : vertices) {
        vertex.SetTangent(Vector3(0.0f));
        vertex.SetBitangent(Vector3(0.0f));
    }

    std::vector<Vector3> new_tangents(vertices.size(), Vector3());
    std::vector<Vector3> new_bitangents(vertices.size(), Vector3());

    for (size_t i = 0; i < indices.size(); i += 3) {
        for (int j = 0; j < 3; j++) {
            v[j] = &vertices[indices[i + j]];
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

        new_tangents[indices[i]] += tangent;
        new_tangents[indices[i + 1]] += tangent;
        new_tangents[indices[i + 2]] += tangent;

        new_bitangents[indices[i]] += bitangent;
        new_bitangents[indices[i + 1]] += bitangent;
        new_bitangents[indices[i + 2]] += bitangent;
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        Vector3 n = vertices[i].GetNormal();
        Vector3 tangent = (new_tangents[i] - (n * n.Dot(new_tangents[i])));
        Vector3 cross = n.Cross(new_tangents[i]);

        Vector3 bitangent = cross * MathUtil::Sign(cross.Dot(new_bitangents[i]));

        vertices[i].SetTangent(tangent);
        vertices[i].SetBitangent(bitangent);
    } 

    EnableAttribute(ATTR_TANGENTS);
    EnableAttribute(ATTR_BITANGENTS);
}

void Mesh::InvertNormals()
{
    for (Vertex &vert : vertices) {
        vert.SetNormal(vert.GetNormal() * -1);
    }
}

std::shared_ptr<Renderable> Mesh::CloneImpl()
{
    auto clone = std::make_shared<Mesh>();

    clone->SetVertices(vertices, indices);
    clone->attribs = attribs;
    clone->primitive_type = primitive_type;
    clone->m_shader = m_shader; // TODO: clone shader?
    clone->m_aabb = m_aabb;

    return clone;
}

} // namespace hyperion
