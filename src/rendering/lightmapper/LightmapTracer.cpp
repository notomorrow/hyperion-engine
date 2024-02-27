#include <rendering/lightmapper/LightmapTracer.hpp>
#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <math/MathUtil.hpp>

#include <util/MeshBuilder.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion::v2 {

LightmapTracer::LightmapTracer(const LightmapTracerParams &params)
    : m_params(params),
      m_noise_generator(0x12345, { 0.0f, 1.0f })
{
    AssertThrowMsg(params.light.IsValid(), "No light provided");
    AssertThrowMsg(params.scene.IsValid(), "No scene provided");
}

void LightmapTracer::PreloadMeshData()
{
    Proc<void, const Handle<Mesh> &, const Transform &> cache_meshes_proc = [this](const Handle<Mesh> &mesh, const Transform &transform) -> void
    {
        if (!mesh.IsValid()) {
            return;
        }

        if (!mesh->GetStreamedMeshData()) {
            return;
        }

        auto ref = mesh->GetStreamedMeshData()->AcquireRef();

        m_mesh_data_cache.elements.Set(mesh.GetID(), std::move(ref));
    };

    CollectMeshes(m_params.scene->GetRoot(), cache_meshes_proc);
}

void LightmapTracer::CollectMeshes(NodeProxy node, Proc<void, const Handle<Mesh> &, const Transform &> &proc)
{
    const ID<Entity> entity = node.GetEntity();

    if (entity.IsValid()) {
        MeshComponent *mesh_component = m_params.scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity);
        TransformComponent *transform_component = m_params.scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity);

        if (mesh_component != nullptr && mesh_component->mesh.IsValid() && transform_component != nullptr) {
            proc(mesh_component->mesh, transform_component->transform);
        }
    }

    for (auto &child : node.GetChildren()) {
        CollectMeshes(child, proc);
    }
}

void LightmapTracer::HandleRayHit(const LightmapRayHit &hit, LightmapHitPath &path, uint depth)
{
    if (depth >= num_bounces) {
        return;
    }

    const ID<Mesh> mesh_id = hit.mesh_id;
    AssertThrowMsg(mesh_id.IsValid(), "Ray hit mesh ID is invalid");

    const ID<Entity> entity_id = hit.entity_id;
    AssertThrowMsg(entity_id.IsValid(), "Ray hit entity ID is invalid");

    const uint triangle_index = hit.triangle_index;
    AssertThrowMsg(triangle_index != ~0u, "Ray hit triangle index is invalid");

    const Vec3f hitpoint = hit.ray_hit.hitpoint;

    const auto it = m_mesh_data_cache.elements.Find(mesh_id);
    if (it == m_mesh_data_cache.elements.End()) {
        DebugLog(
            LogType::Warn,
            "Mesh with ID %u not found in mesh data cache!\n",
            mesh_id.Value()
        );

        return;
    }

    MeshComponent *mesh_component = m_params.scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
    AssertThrowMsg(mesh_component != nullptr, "Mesh component is null");
    AssertThrowMsg(mesh_component->mesh.IsValid(), "Mesh is invalid");

    const MeshData &mesh_data = it->second->GetMeshData();
    const BoundingBox &mesh_aabb = mesh_component->mesh->GetAABB();

    TransformComponent *transform_component = m_params.scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);
    AssertThrowMsg(transform_component != nullptr, "Transform component is null");

    const Matrix4 inverse_model_matrix = transform_component->transform.GetMatrix().Inverted();

    const Vec3f local_hitpoint = inverse_model_matrix * hitpoint;

    AssertThrowMsg(triangle_index * 3 + 2 < mesh_data.indices.Size(), "Triangle index out of bounds (%u >= %llu)\n", triangle_index * 3 + 2, mesh_data.indices.Size());

    const Vertex triangle_vertices[3] = {
        mesh_data.vertices[mesh_data.indices[triangle_index * 3 + 0]],
        mesh_data.vertices[mesh_data.indices[triangle_index * 3 + 1]],
        mesh_data.vertices[mesh_data.indices[triangle_index * 3 + 2]]
    };

    const Vec3f barycentric_coordinates = MathUtil::CalculateBarycentricCoordinates(
        triangle_vertices[0].position,
        triangle_vertices[1].position,
        triangle_vertices[2].position,
        local_hitpoint
    );

    const Vec3f position = triangle_vertices[0].position * barycentric_coordinates.x
        + triangle_vertices[1].position * barycentric_coordinates.y
        + triangle_vertices[2].position * barycentric_coordinates.z;

    const Vec2f uv = triangle_vertices[0].texcoord0 * barycentric_coordinates.x
        + triangle_vertices[1].texcoord0 * barycentric_coordinates.y
        + triangle_vertices[2].texcoord0 * barycentric_coordinates.z;

    const Vec3f normal = triangle_vertices[0].normal * barycentric_coordinates.x
        + triangle_vertices[1].normal * barycentric_coordinates.y
        + triangle_vertices[2].normal * barycentric_coordinates.z;

    const Vec3f tangent = triangle_vertices[0].tangent * barycentric_coordinates.x
        + triangle_vertices[1].tangent * barycentric_coordinates.y
        + triangle_vertices[2].tangent * barycentric_coordinates.z;

    const Vec3f bitangent = triangle_vertices[0].bitangent * barycentric_coordinates.x
        + triangle_vertices[1].bitangent * barycentric_coordinates.y
        + triangle_vertices[2].bitangent * barycentric_coordinates.z;

    const Handle<Material> &material = mesh_component->material;
    AssertThrow(material.IsValid());

    Vec4f albedo = Vec4f(material->GetParameter(Material::MATERIAL_KEY_ALBEDO));

    if (const Handle<Texture> &albedo_texture = material->GetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP); albedo_texture.IsValid()) {
        const Vec4f albedo_texture_sample = albedo_texture->Sample(uv);

        albedo *= albedo_texture_sample;
    }

    float roughness = float(material->GetParameter(Material::MATERIAL_KEY_ROUGHNESS));

    // if (const Handle<Texture> &roughness_texture = material->GetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP); roughness_texture.IsValid()) {
    //     const float roughness_texture_sample = roughness_texture->Sample(uv).x;

    //     roughness = roughness_texture_sample;
    // }

    path.AddHit({
        .hitpoint       = hitpoint,
        .barycentric    = barycentric_coordinates,
        .throughput     = albedo,
        .emissive       = 0.0f,
        .mesh_id        = mesh_id,
        .triangle_index = triangle_index
    });

    if (depth + 1 < num_bounces) {
        const Vec2f rnd {
            m_noise_generator.Next(),
            m_noise_generator.Next()
        };

        Vec3f H = MathUtil::ImportanceSampleGGX(rnd, normal, roughness);
        H = tangent * H.x + bitangent * H.y + normal * H.z;
        H.Normalize();

        const Vec3f R = hit.ray.direction.Reflect(H).Normalized();

        const Ray ray { hit.ray_hit.hitpoint + normal * 0.25f, R };

        const Optional<LightmapRayHit> hit = TraceSingleRay(ray);

        if (hit.HasValue()) {
            HandleRayHit(hit.Get(), path, depth + 1);
        }
    }
}

LightmapTracer::Result LightmapTracer::Trace()
{
    LightmapTraceData trace_data;

    const Vec3f scene_origin = m_params.scene->GetOctree().GetAABB().GetCenter();
    Vec3f light_position;
    
    LightmapUVBuilder uv_builder;
    LightmapUVBuilderParams uv_builder_params;

    switch (m_params.light->GetType()) {
    case LightType::DIRECTIONAL: {
        const Vec3f light_direction = m_params.light->GetPosition().Normalized();
        light_position = scene_origin + (light_direction * 10.0f);

        Proc<void, const Handle<Mesh> &, const Transform &> proc = [this, &trace_data, &uv_builder_params, light_direction](const Handle<Mesh> &mesh, const Transform &transform) -> void
        {
            PerformTracingOnMesh(mesh, transform, trace_data, light_direction);

            uv_builder_params.elements.PushBack({
                mesh
            });
        };

        CollectMeshes(m_params.scene->GetRoot(), proc);

        break;
    }
    case LightType::POINT:
        light_position = m_params.light->GetPosition();

        // @TODO

        break;
    default:
        return { Result::RESULT_ERR, "LightmapTracer cannot trace the given light type" };
    }

    LightmapUVBuilder::Result uv_builder_result = uv_builder.Build(uv_builder_params);

    if (!uv_builder_result) {
        return { Result::RESULT_ERR, uv_builder_result.message };
    }

    for (const auto &it : uv_builder_result.result.mesh_uvs) {
        const ID<Mesh> mesh_id = it.first;
        const LightmapMeshUVs &mesh_uvs = it.second;

        const auto trace_data_it = trace_data.hits_by_mesh_id.Find(mesh_id);

        if (trace_data_it == trace_data.hits_by_mesh_id.End()) {
            continue;
        }

        Handle<Mesh> mesh = Handle<Mesh>(mesh_id);
        AssertThrow(mesh.IsValid());

        auto ref = mesh->GetStreamedMeshData()->AcquireRef();
        AssertThrow(mesh_uvs.uvs.Size() == ref->GetMeshData().vertices.Size());

        // Set lightmap uv in mesh vertex data

        MeshData new_mesh_data;
        new_mesh_data.vertices = ref->GetMeshData().vertices;
        new_mesh_data.indices = ref->GetMeshData().indices;

        for (SizeType i = 0; i < new_mesh_data.vertices.Size(); i++) {
            const Vec2f lightmap_uv = mesh_uvs.uvs[i];

            new_mesh_data.vertices[i].SetTexCoord1(lightmap_uv);
        }

        // Write lightmap to file
        AssertThrow(uv_builder_result.result.bitmap != nullptr);

        const uint width = uv_builder_result.result.bitmap->GetWidth();
        const uint height = uv_builder_result.result.bitmap->GetHeight();

        for (const auto &hit : trace_data_it->second) {
            const LightmapHitData &hit_data = hit.second;

            const uint triangle_index = hit_data.triangle_index;

            AssertThrow(triangle_index * 3 + 2 < new_mesh_data.indices.Size());

            const Vec2f &uv0 = mesh_uvs.uvs[new_mesh_data.indices[triangle_index * 3 + 0]];
            const Vec2f &uv1 = mesh_uvs.uvs[new_mesh_data.indices[triangle_index * 3 + 1]];
            const Vec2f &uv2 = mesh_uvs.uvs[new_mesh_data.indices[triangle_index * 3 + 2]];

            const Vec2f uv = uv0 * hit_data.barycentric.x + uv1 * hit_data.barycentric.y + uv2 * hit_data.barycentric.z;
            const Vec2u coord = Vec2u(uint(uv.x * float(width)) % width, uint(uv.y * float(height)) % height);

            uv_builder_result.result.bitmap->SetPixel(coord.x, coord.y, Vec3f { hit_data.throughput.x, hit_data.throughput.y, hit_data.throughput.z });
        }

        Mesh::SetStreamedMeshData(mesh, StreamedMeshData::FromMeshData(new_mesh_data));
    }

    uv_builder_result.result.bitmap->Write("lightmap.bmp");


        // auto ref = mesh->GetStreamedMeshData()->AcquireRef();

        // MeshData new_mesh_data;
        // new_mesh_data.vertices = ref->GetMeshData().vertices;
        // new_mesh_data.indices = ref->GetMeshData().indices;

        // // TEMP
        // for (uint i = 0; i < new_mesh_data.vertices.Size(); i++) {
        //     new_mesh_data.vertices[i].SetTangent(Vec3f{ 0.0f, 0.0f, 0.0f });
        // }

        // for (const auto &hit : it.second) {
        //     const uint triangle_index = hit.second.triangle_index;
        //     const Vec4f throughput = hit.second.throughput;

        //     // TEMP
        //     AssertThrowMsg(triangle_index * 3 + 2 < new_mesh_data.indices.Size(), "Index out of bounds (%u >= %llu)\n", triangle_index * 3 + 2, new_mesh_data.indices.Size());
        //     new_mesh_data.vertices[new_mesh_data.indices[triangle_index * 3 + 0]].SetTangent(throughput.GetXYZ());
        //     new_mesh_data.vertices[new_mesh_data.indices[triangle_index * 3 + 1]].SetTangent(throughput.GetXYZ());
        //     new_mesh_data.vertices[new_mesh_data.indices[triangle_index * 3 + 2]].SetTangent(throughput.GetXYZ());
        // }

        // Mesh::SetStreamedMeshData(mesh, StreamedMeshData::FromMeshData(new_mesh_data));


    return {
        Result::RESULT_OK
    };
}

void LightmapTracer::PerformTracingOnMesh(const Handle<Mesh> &mesh, const Transform &transform, LightmapTraceData &trace_data, Vec3f light_direction)
{
    if (!mesh.IsValid()) {
        return;
    }

    if (!mesh->GetStreamedMeshData()) {
        return;
    }

    auto ref = mesh->GetStreamedMeshData()->AcquireRef();
    m_mesh_data_cache.elements.Set(mesh.GetID(), ref);

    const MeshData &mesh_data = ref->GetMeshData();

    DebugLog(LogType::Debug, "Performing tracing on mesh with ID %u, %llu vertices, %llu indices\n", mesh.GetID().Value(), mesh_data.vertices.Size(), mesh_data.indices.Size());

    LightmapHitPath path;

    for (uint i = 0; i < mesh_data.indices.Size(); i += 3) {
        const Triangle triangle {
            mesh_data.vertices[mesh_data.indices[i + 0]].GetPosition() * transform.GetMatrix(),
            mesh_data.vertices[mesh_data.indices[i + 1]].GetPosition() * transform.GetMatrix(),
            mesh_data.vertices[mesh_data.indices[i + 2]].GetPosition() * transform.GetMatrix()
        };

        const Vec3f normal = triangle.GetNormal();

        const Ray ray { triangle.GetPosition() + normal * 0.25f, -normal };

        const Optional<LightmapRayHit> hit = TraceSingleRay(ray);

        if (hit.HasValue()) {
            DebugLog(LogType::Debug, "Hit triangle %u\n", i / 3);

            HandleRayHit(hit.Get(), path);
        }
    }
}

Optional<LightmapRayHit> LightmapTracer::TraceSingleRay(const Ray &ray)
{
    RayTestResults octree_results;

    if (m_params.scene->GetOctree().TestRay(ray, octree_results)) {
        FlatSet<LightmapRayHit> results;

        for (const auto &hit : octree_results) {
            // now ray test each result as triangle mesh to find exact hit point
            if (ID<Entity> entity_id = ID<Entity>(hit.id)) {
                MeshComponent *mesh_component = m_params.scene->GetEntityManager()->TryGetComponent<MeshComponent>(entity_id);
                TransformComponent *transform_component = m_params.scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

                if (mesh_component != nullptr && mesh_component->mesh.IsValid() && mesh_component->mesh->NumIndices() != 0 && transform_component != nullptr) {
                    const ID<Mesh> mesh_id = mesh_component->mesh.GetID();

                    auto mesh_data_cache_it = m_mesh_data_cache.elements.Find(mesh_id);

                    if (mesh_data_cache_it == m_mesh_data_cache.elements.End()) {
                        auto ref = mesh_component->mesh->GetStreamedMeshData()->AcquireRef();

                        mesh_data_cache_it = m_mesh_data_cache.elements.Insert(mesh_component->mesh.GetID(), std::move(ref)).first;
                    }

                    const MeshData &mesh_data = mesh_data_cache_it->second->GetMeshData();

                    const Optional<RayHit> hit = ray.TestTriangleList(
                        mesh_data.vertices,
                        mesh_data.indices,
                        transform_component->transform
                    );

                    if (hit.HasValue()) {
                        results.Insert({
                            entity_id,  // entity_id
                            mesh_id,    // mesh_id
                            hit->id,    // triangle_index
                            hit.Get(),  // ray_hit
                            ray         // ray
                        });
                    }
                }
            }
        }

        if (!results.Empty()) {
            return results.Front();
        }
    }

    return { };
}

} // namespace hyperion::v2