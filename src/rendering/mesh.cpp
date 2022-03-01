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
      vao(0),
      vbo(0),
      ibo(0),
      vk_vbo(nullptr),
      vk_ibo(nullptr)
{
    EnableAttribute(ATTR_POSITIONS);
    EnableAttribute(ATTR_NORMALS);
    EnableAttribute(ATTR_TEXCOORDS0);
    EnableAttribute(ATTR_TEXCOORDS1);
    EnableAttribute(ATTR_TANGENTS);
    EnableAttribute(ATTR_BITANGENTS);
    SetPrimitiveType(PRIM_TRIANGLES);
    DebugLog(LogType::Info, "Calling Mesh constructor\n");
    is_uploaded = false;
    is_created = false;
}

Mesh::~Mesh()
{
    DebugLog(LogType::Info, "Calling Mesh destructor\n");

    /*
    if (is_created) {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ibo);
    }*/

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
    ex_assert(indices.size() % 3 == 0);

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

void Mesh::SetAttribute(MeshAttributeType type, const MeshAttribute &attribute)
{
    attribs[type] = attribute;
    is_uploaded = false;
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

void Mesh::CalculateTangents()
{
    Vertex *v[3];
    Vector2 uv[3];

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

        if (cp != 0.0f) {
            const float mul = 1.0f / cp;

            Vector3 tangent;
            tangent.x = edge2uv.y * edge1.x - edge1uv.y * edge2.x;
            tangent.y = edge2uv.y * edge1.y - edge1uv.y * edge2.y;
            tangent.z = edge2uv.y * edge1.z - edge1uv.y * edge2.z;
            tangent *= mul;
            tangent.Normalize();

            Vector3 bitangent;

            bitangent.x = -edge2uv.x * edge1.x + edge1uv.x * edge2.x;
            bitangent.y = -edge2uv.x * edge1.y + edge1uv.x * edge2.y;
            bitangent.z = -edge2uv.x * edge1.z + edge1uv.x * edge2.z;
            bitangent *= mul;
            bitangent.Normalize();

            for (int j = 0; j < 3; j++) {
                v[j]->SetTangent(tangent);
                v[j]->SetBitangent(bitangent);
            }
        }
    }

    EnableAttribute(ATTR_TANGENTS);
    EnableAttribute(ATTR_BITANGENTS);
}

RendererMeshBindingDescription Mesh::GetBindingDescription() {
    auto desc = RendererMeshBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    return desc;
}

void Mesh::Render(Renderer *renderer, Camera *cam) {

}

void Mesh::RenderVk(VkCommandBuffer *cmd, VkRenderer *vk_renderer, Camera *cam) {
    if (!this->is_created) {
        AssertThrow(this->vk_vbo == nullptr/* || this->vk_ibo == nullptr*/);
        this->vk_vbo = new RendererGPUBuffer();
        this->vk_ibo = new RendererGPUBuffer();

        this->is_created = true;
    }
    RendererPipeline *pipeline = vk_renderer->GetCurrentPipeline();
    //VkCommandBuffer *cmd = &pipeline->command_buffers[image_index];
    vkCmdBindPipeline(*cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    const VkDeviceSize offsets[] = { 0 };

    RendererDevice *device = vk_renderer->GetRendererDevice();
    VkDevice vk_device = device->GetDevice();
    if (!this->is_uploaded) {
        std::vector<float> buffer = CreateBuffer();
        const size_t gpu_buffer_size = buffer.size() * sizeof(float);

        /* Bind and copy vertex buffer */
        this->vk_vbo->Create(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, gpu_buffer_size);
        vkCmdBindVertexBuffers(*cmd, 0, 1, &this->vk_vbo->buffer, offsets);

        void *memory_buffer = nullptr;
        vkMapMemory(vk_device, this->vk_vbo->memory, 0, gpu_buffer_size, 0, &memory_buffer);
        memcpy(memory_buffer, &buffer[0], gpu_buffer_size);
        vkUnmapMemory(vk_device, this->vk_vbo->memory);

        for (auto &index : this->indices) {
            DebugLog(LogType::Debug, "Index : %d\n", index);
        }
        /* Bind and copy index buffer */
        size_t gpu_indices_size = indices.size()*sizeof(MeshIndex);
        this->vk_ibo->SetSharingMode(VK_SHARING_MODE_EXCLUSIVE);
        this->vk_ibo->Create(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, gpu_indices_size);
        vkCmdBindIndexBuffer(*cmd, this->vk_ibo->buffer, 0, VK_INDEX_TYPE_UINT32);

        memory_buffer = nullptr;
        vkMapMemory(vk_device, this->vk_ibo->memory, 0, gpu_indices_size, 0, &memory_buffer);
        memcpy(memory_buffer, indices.data(), gpu_indices_size);
        vkUnmapMemory(vk_device, this->vk_ibo->memory);

        this->is_uploaded = true;
    }
    AssertThrow(this->vk_vbo->buffer != nullptr);

    vkCmdBindVertexBuffers(*cmd, 0, 1, &this->vk_vbo->buffer, offsets);
    vkCmdBindIndexBuffer(*cmd, this->vk_ibo->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(*cmd, (indices.size()), 1, 0, 0, 0);
    //vkCmdDraw(*cmd, vertices.size(), 1, 0, 0);

    //vkCmdDraw(*cmd, 3, 1, 0, 0);
    DebugLog(LogType::Info, "DRAW %d\n", this->indices.size());
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

        //vertices[i0].SetNormal(vertices[i0].GetNormal() + n);
        //vertices[i1].SetNormal(vertices[i1].GetNormal() + n);
        //vertices[i2].SetNormal(vertices[i2].GetNormal() + n);
        /*new_normals[i] = n;
        new_normals[i + 1] = n;
        new_normals[i + 2] = n;*/
        normals[i0].push_back(n);
        normals[i1].push_back(n);
        normals[i2].push_back(n);
    }

    /*for (Vertex &vert : vertices) {
        vert.SetNormal(Vector3(vert.GetNormal()).Normalize());
    }*/

    for (size_t i = 0; i < vertices.size(); i++) {
        // find average
        Vector3 average;

        for (const auto &normal : normals[i]) {
            average += normal * (1.0f / float(normals[i].size()));
        }

        average.Normalize();

        vertices[i].SetNormal(average);

        /*int shared = 0;
        Vector3 normal_sum;

        for (size_t j = 0; j < indices.size(); j++) {
            if (indices[j] != i) {
                continue;
            }

            normal_sum += new_normals[j];
            shared++;
        }

        if (shared) {
            vertices[i].SetNormal(Vector3(normal_sum / shared).Normalize());
        }*/
    }

    EnableAttribute(ATTR_NORMALS);

    CalculateTangents();
}

void Mesh::CalculateTangents()
{
    /*Vertex *v[3];
    Vector2 uv[3];

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

        if (cp != 0.0f) {
            const float mul = 1.0f / cp;

            Vector3 tangent;
            tangent.x = edge2uv.y * edge1.x - edge1uv.y * edge2.x;
            tangent.y = edge2uv.y * edge1.y - edge1uv.y * edge2.y;
            tangent.z = edge2uv.y * edge1.z - edge1uv.y * edge2.z;
            tangent *= mul;
            tangent.Normalize();

            Vector3 bitangent;

            bitangent.x = -edge2uv.x * edge1.x + edge1uv.x * edge2.x;
            bitangent.y = -edge2uv.x * edge1.y + edge1uv.x * edge2.y;
            bitangent.z = -edge2uv.x * edge1.z + edge1uv.x * edge2.z;
            bitangent *= mul;
            bitangent.Normalize();

            for (int j = 0; j < 3; j++) {
                v[j]->SetTangent(tangent);
                v[j]->SetBitangent(bitangent);
            }
        }
    }*/


    /*Vertex *v[3];
    Vector2 uv[3];

    for (auto &vertex : vertices) {
        vertex.SetTangent(Vector3(0.0f));
        vertex.SetBitangent(Vector3(0.0f));
    }

    std::vector<Vector3> new_tangents(indices.size(), Vector3());
    std::vector<Vector3> new_bitangents(indices.size(), Vector3());

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

        new_tangents[i] += tangent;
        new_tangents[i + 1] += tangent;
        new_tangents[i + 2] += tangent;

        new_bitangents[i] += bitangent;
        new_bitangents[i + 1] += bitangent;
        new_bitangents[i + 2] += bitangent;
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        int shared = 0;
        Vector3 tangent_sum, bitangent_sum;

        for (size_t j = 0; j < indices.size(); j++) {
            if (indices[j] != i) {
                continue;
            }

            tangent_sum += new_tangents[j];
            bitangent_sum += new_bitangents[j];
            shared++;
        }

        if (shared) {
            vertices[i].SetTangent(Vector3(tangent_sum / shared).Normalize());
            vertices[i].SetBitangent(Vector3(bitangent_sum / shared).Normalize());
        }
    }*/

    /*for (size_t i = 0; i < vertices.size(); i++) {
        Vector3 n = vertices[i].GetNormal();
        Vector3 tangent = (new_tangents[i] - (n * n.Dot(new_tangents[i])));
        Vector3 cross = n.Cross(new_tangents[i]);

        Vector3 bitangent = cross * MathUtil::Sign(cross.Dot(new_bitangents[i]));

        vertices[i].SetTangent(tangent);

        vertices[i].SetBitangent(bitangent);
    }*/


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

        //if (!MathUtil::Approximately(cp, 0.0f)) {
            const float mul = 1.0f / cp;

            Vector3 tangent = ((edge1 * edge2uv.y) - (edge2 * edge1uv.y)) * mul;
            Vector3 bitangent = ((edge1 * edge2uv.x) - (edge2 * edge1uv.x)) * mul;

            new_tangents[indices[i]] += tangent;
            new_tangents[indices[i + 1]] += tangent;
            new_tangents[indices[i + 2]] += tangent;

            new_bitangents[indices[i]] += bitangent;
            new_bitangents[indices[i + 1]] += bitangent;
            new_bitangents[indices[i + 2]] += bitangent;
        //}
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        Vector3 n = vertices[i].GetNormal();
        Vector3 tangent = (new_tangents[i] - (n * n.Dot(new_tangents[i])));
        Vector3 cross = n.Cross(new_tangents[i]);

        Vector3 bitangent = cross * MathUtil::Sign(cross.Dot(new_bitangents[i]));

        vertices[i].SetTangent(tangent);
        vertices[i].SetBitangent(bitangent);
    } 

    SetAttribute(ATTR_TANGENTS, MeshAttribute::Tangents);
    SetAttribute(ATTR_BITANGENTS, MeshAttribute::Bitangents);
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
