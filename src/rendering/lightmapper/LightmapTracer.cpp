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
    AssertThrowMsg(false, "Not implemented");
}

LightmapTracer::Result LightmapTracer::Trace()
{
    LightmapTraceData trace_data;

    const Vec3f scene_origin = m_params.scene->GetOctree().GetAABB().GetCenter();
    Vec3f light_position;
    
    LightmapUVBuilderParams uv_builder_params;

    switch (m_params.light->GetType()) {
    case LightType::DIRECTIONAL: {
        const Vec3f light_direction = m_params.light->GetPosition().Normalized();
        light_position = scene_origin + (light_direction * 10.0f);

        Proc<void, const Handle<Mesh> &, const Transform &> proc = [this, &trace_data, &uv_builder_params, light_direction](const Handle<Mesh> &mesh, const Transform &transform) -> void
        {
            PerformTracingOnMesh(mesh, transform, trace_data, light_direction);

            // @TODO
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
    
    LightmapUVBuilder uv_builder { uv_builder_params };
    LightmapUVBuilder::Result uv_builder_result = uv_builder.Build();

    if (!uv_builder_result) {
        return { Result::RESULT_ERR, uv_builder_result.message };
    }


    // @TODO - Perform lightmap baking

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