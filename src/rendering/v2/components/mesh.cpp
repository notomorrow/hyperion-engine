//
// Created by emd22 on 2022-03-19.
//

#include "mesh.h"

#include "../engine.h"

#include <rendering/backend/renderer_command_buffer.h>

#include <vector>
#include <map>
#include <cstring>

#define HYP_MESH_AABB_USE_MULTITHREADING 1

#if HYP_MESH_AABB_USE_MULTITHREADING
#include <thread>
#endif

namespace hyperion::v2 {

std::pair<std::vector<Vertex>, std::vector<Mesh::Index>>
Mesh::CalculateIndices(const std::vector<Vertex> &vertices)
{
    std::map<Vertex, Index> index_map;

    std::vector<Index> indices;
    indices.reserve(vertices.size());

    /* This will be our resulting buffer with only the vertices we need. */
    std::vector<Vertex> new_vertices;
    new_vertices.reserve(vertices.size());

    for (const auto &vertex : vertices) {
        /* Check if the vertex already exists in our map */
        auto it = index_map.find(vertex);

        /* If it does, push to our indices */
        if (it != index_map.end()) {
            indices.push_back(it->second);

            continue;
        }

        const auto mesh_index = static_cast<Index>(new_vertices.size());

        /* The vertex is unique, so we push it. */
        index_map[vertex] = mesh_index;
        new_vertices.push_back(vertex);

        indices.push_back(mesh_index);
    }

    return std::make_pair(new_vertices, indices);
}

Mesh::Mesh(
    const std::vector<Vertex> &vertices,
    const std::vector<Index> &indices,
    Flags flags
)
    : EngineComponentBase(),
      m_vertices(std::move(vertices)),
      m_indices(std::move(indices)),
      m_vertex_attributes(
          MeshInputAttribute::MESH_INPUT_ATTRIBUTE_POSITION
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_NORMAL
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES
          | MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS),
      m_flags(flags)
{
}

Mesh::~Mesh()
{
    Teardown();
}

void Mesh::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_MESHES, [this](Engine *engine) {
        DebugLog(LogType::Info, "Init mesh with %llu vertices and %llu indices\n", m_vertices.size(), m_indices.size());

        if (m_vertices.empty() || m_indices.empty()) {
            DebugLog(LogType::Warn, "Attempt to create Mesh #%lu with empty vertices or indices list\n", m_id.value);

            /* set to 1 vertex / index to prevent explosions */
            m_vertices = {Vertex()};
            m_indices  = {0};
        }

        Upload(engine->GetInstance());

        if (m_flags & MESH_FLAGS_HAS_ACCELERATION_GEOMETRY) {
            CreateAccelerationGeometry(engine);
        }

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_MESHES, [this](Engine *engine) {
            AssertThrow(m_vbo != nullptr);
            AssertThrow(m_ibo != nullptr);

            Device *device = engine->GetInstance()->GetDevice();

            if (m_acceleration_geometry != nullptr) {
                m_acceleration_geometry->Destroy(device);
                m_acceleration_geometry.reset();
            }
            
            m_vbo->Destroy(device);
            m_vbo.reset();

            m_ibo->Destroy(device);
            m_ibo.reset();
        }), engine);
    }));
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                    \
    do {                                                                                         \
        memcpy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                            \
    } while (0)

std::vector<float> Mesh::CreatePackedBuffer() {
    const size_t vertex_size = m_vertex_attributes.CalculateVertexSize();

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
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1) PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)   PACKED_SET_ATTR(vertex.GetTangent().values,   3);
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT) PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS) {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }

        if (m_vertex_attributes & MeshInputAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES) {
            float indices[4] = {
                    (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                    (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, std::size(indices));
        }
    }

    return packed_buffer;
}

#undef PACKED_SET_ATTR

void Mesh::Upload(Instance *instance)
{
    using renderer::StagingBuffer;

    auto *device = instance->GetDevice();

    AssertThrow(m_vbo == nullptr);
    AssertThrow(m_ibo == nullptr);
    
    m_vbo = std::make_unique<VertexBuffer>();
    m_ibo = std::make_unique<IndexBuffer>();

    std::vector<float> packed_buffer = CreatePackedBuffer();
    const size_t packed_buffer_size  = packed_buffer.size() * sizeof(float);
    const size_t packed_indices_size = m_indices.size() * sizeof(Index);

    HYPERION_ASSERT_RESULT(m_vbo->Create(device, packed_buffer_size));
    HYPERION_ASSERT_RESULT(m_ibo->Create(device, packed_indices_size));

    HYPERION_ASSERT_RESULT(instance->GetStagingBufferPool().Use(device, [&](renderer::StagingBufferPool::Context &holder) {
        auto commands = instance->GetSingleTimeCommands();

        auto *staging_buffer_vertices = holder.Acquire(packed_buffer_size);
        /* Create and upload the VBO */
        staging_buffer_vertices->Copy(device, packed_buffer_size, packed_buffer.data());

        auto *staging_buffer_indices = holder.Acquire(packed_indices_size);
        staging_buffer_indices->Copy(device, packed_indices_size, m_indices.data());


        commands.Push([&](CommandBuffer *cmd) {
            m_vbo->CopyFrom(cmd, staging_buffer_vertices, packed_buffer_size);

            HYPERION_RETURN_OK;
        });
    
        commands.Push([&](CommandBuffer *cmd) {
            m_ibo->CopyFrom(cmd, staging_buffer_indices, packed_indices_size);

            HYPERION_RETURN_OK;
        });
    
        HYPERION_BUBBLE_ERRORS(commands.Execute(device));

        HYPERION_RETURN_OK;
    }));
}

void Mesh::Render(Engine *engine, CommandBuffer *cmd) const
{
    AssertThrow(m_vbo != nullptr && m_ibo != nullptr);

    m_vbo->Bind(cmd);
    m_ibo->Bind(cmd);

    vkCmdDrawIndexed(
        cmd->GetCommandBuffer(),
        uint32_t(m_indices.size()),
        1, 0, 0, 0
    );
}

void Mesh::CreateAccelerationGeometry(Engine *engine)
{
    if (m_acceleration_geometry != nullptr) {
        HYPERION_ASSERT_RESULT(
            m_acceleration_geometry->Destroy(engine->GetDevice())
        );
    }
    
    AssertThrow(m_vbo != nullptr);
    AssertThrow(m_ibo != nullptr);

    m_acceleration_geometry = std::make_unique<AccelerationGeometry>(
        m_vbo.get(),
        m_vertices.size(),
        m_vertex_attributes.CalculateVertexSize(),
        m_ibo.get(),
        m_indices.size()
    );
    
    HYPERION_ASSERT_RESULT(
        m_acceleration_geometry->Create(engine->GetDevice())
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

        const Vector3 u = p2 - p0;
        const Vector3 v = p1 - p0;
        const Vector3 n = v.Cross(u).Normalize();

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

BoundingBox Mesh::CalculateAabb() const
{
    BoundingBox aabb;

#if HYP_MESH_AABB_USE_MULTITHREADING
    constexpr size_t max_threads = 8;
    constexpr size_t vertex_count_threshold = 512;

    if (m_vertices.size() > vertex_count_threshold) {
        std::vector<std::thread> threads;
        threads.reserve(max_threads);

        const int vertex_stride = MathUtil::Ceil(double(m_vertices.size()) / double(max_threads));

        std::array<BoundingBox, max_threads> working_aabbs;

        for (size_t i = 0; i < max_threads; i++) {
            const size_t vertex_offset = vertex_stride * i;

            threads.emplace_back([this, &working_aabbs, vertex_offset, vertex_stride, i] {
                const size_t vertex_end = MathUtil::Min(m_vertices.size(), vertex_offset + vertex_stride);
                
                for (size_t j = vertex_offset; j < vertex_end; j++) {
                    working_aabbs[i].Extend(m_vertices[j].GetPosition());
                }
            });
        }

        for (size_t i = 0; i < threads.size(); i++) {
            threads[i].join();

            aabb.Extend(working_aabbs[i]);
        }
    } else {
#endif
        for (const Vertex &vertex : m_vertices) {
            aabb.Extend(vertex.GetPosition());
        }
#if HYP_MESH_AABB_USE_MULTITHREADING
    }
#endif

    return aabb;
}

}