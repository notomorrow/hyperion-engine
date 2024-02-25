#include <rendering/lightmapper/LightmapTracer.hpp>

#include <math/MathUtil.hpp>

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

void LightmapTracer::HandleRayHit(const LightmapRayHit &hit, uint depth)
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
    TransformComponent *transform_component = m_params.scene->GetEntityManager()->TryGetComponent<TransformComponent>(entity_id);

    AssertThrowMsg(mesh_component != nullptr, "Mesh component is null");
    AssertThrowMsg(transform_component != nullptr, "Transform component is null");

    const Matrix4 inverse_model_matrix = transform_component->transform.GetMatrix().Inverted();

    Vec3f local_hitpoint = inverse_model_matrix * hitpoint;

    const MeshData &mesh_data = it->second->GetMeshData();

    const Vertex triangle_vertices[3] = {
        mesh_data.vertices[mesh_data.indices[triangle_index + 0]],
        mesh_data.vertices[mesh_data.indices[triangle_index + 1]],
        mesh_data.vertices[mesh_data.indices[triangle_index + 2]]
    };

    const Vec3f barycentric_coordinates = MathUtil::CalculateBarycentricCoordinates(
        triangle_vertices[0].position,
        triangle_vertices[1].position,
        triangle_vertices[2].position,
        local_hitpoint
    );

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

    if (const Handle<Texture> &roughness_texture = material->GetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP); roughness_texture.IsValid()) {
        const float roughness_texture_sample = roughness_texture->Sample(uv).x;

        roughness = roughness_texture_sample;
    }

    DebugLog(
        LogType::Debug,
        "Depth: %u\tAlbedo: %f %f %f %f\n",
        depth,
        albedo.x, albedo.y, albedo.z, albedo.w
    );

    if (depth + 1 < num_bounces) {
        const Vec2f rnd {
            m_noise_generator.Next(),
            m_noise_generator.Next()
        };

        Vec3f H = MathUtil::ImportanceSampleGGX(rnd, normal, roughness);
        H = tangent * H.x + bitangent * H.y + normal * H.z;
        H.Normalize();

        const Vec3f R = hit.ray.direction.Reflect(H).Normalized();

        const Ray ray { hit.ray_hit.hitpoint + normal * 0.1f, -R };

        const Optional<LightmapRayHit> hit = TraceSingleRay(ray);

        if (hit.HasValue()) {
            HandleRayHit(hit.Get(), depth + 1);
        }
    }
}

LightmapTracer::Result LightmapTracer::Trace()
{
    const Vec3f scene_origin = m_params.scene->GetOctree().GetAABB().GetCenter();

    Vec3f light_position;

    switch (m_params.light->GetType()) {
    case LightType::DIRECTIONAL: {
        const Vec3f light_direction = m_params.light->GetPosition().Normalized();
        light_position = scene_origin + (light_direction * 10.0f);

        for (uint ray_index = 0; ray_index < num_rays_per_light; ray_index++) {
            const Vec3f rnd = { m_noise_generator.Next(), m_noise_generator.Next(), m_noise_generator.Next() };
            const Vec3f v = MathUtil::RandomInHemisphere(rnd, light_direction).Normalized();

            const Ray ray { light_position, -v };

            const Optional<LightmapRayHit> hit = TraceSingleRay(ray);

            if (hit.HasValue()) {
                DebugLog(LogType::Debug, "\tRay hit mesh %u\ttriangle %u\n", hit->mesh_id.Value(), hit->triangle_index);
                
                HandleRayHit(hit.Get());
            } else {
                DebugLog(LogType::Debug, "\tRay missed\n");
            }
        }

        break;
    }
    case LightType::POINT:
        light_position = m_params.light->GetPosition();

        // @TODO

        break;
    default:
        return { Result::RESULT_ERR, "LightmapTracer cannot trace the given light type" };
    }

    return {
        Result::RESULT_OK
    };
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
    } else {
        DebugLog(
            LogType::Debug,
            "Octree ray test failed\n"
        );
    }

    return { };
}

} // namespace hyperion::v2