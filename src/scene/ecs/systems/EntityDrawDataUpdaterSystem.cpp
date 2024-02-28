#include <scene/ecs/systems/EntityDrawDataUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RenderCommand.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(UpdateEntityDrawDatas) : renderer::RenderCommand
{
    Array<EntityDrawData> entity_draw_datas;

    RENDER_COMMAND(UpdateEntityDrawDatas)(Array<EntityDrawData> &&entity_draw_datas)
        : entity_draw_datas(std::move(entity_draw_datas))
    {
    }

    virtual Result operator()()
    {
        for (const EntityDrawData &entity_draw_data : entity_draw_datas) {
            // @TODO: Change it to use EntityDrawData instead of ObjectShaderData.
            g_engine->GetRenderData()->objects.Set(entity_draw_data.entity_id.ToIndex(), ObjectShaderData {
                .model_matrix = entity_draw_data.model_matrix,
                .previous_model_matrix = entity_draw_data.previous_model_matrix,
                .world_aabb_max = Vector4(entity_draw_data.aabb.max, 1.0f),
                .world_aabb_min = Vector4(entity_draw_data.aabb.min, 1.0f),
                .entity_index = entity_draw_data.entity_id.ToIndex(),
                .material_index = entity_draw_data.material_id.ToIndex(),
                .skeleton_index = entity_draw_data.skeleton_id.ToIndex(),
                .bucket = entity_draw_data.bucket,
                .flags = entity_draw_data.skeleton_id ? ENTITY_GPU_FLAG_HAS_SKELETON : ENTITY_GPU_FLAG_NONE
            });
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

void EntityDrawDataUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    InitObject(mesh_component.mesh);
    InitObject(mesh_component.material);

    mesh_component.flags |= MESH_COMPONENT_FLAG_INIT | MESH_COMPONENT_FLAG_DIRTY;
}

void EntityDrawDataUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);

    mesh_component.flags &= ~MESH_COMPONENT_FLAG_INIT;
}

void EntityDrawDataUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    Array<EntityDrawData> entity_draw_datas;

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component] : entity_manager.GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent>()) {
        //if (!(mesh_component.flags & MESH_COMPONENT_FLAG_DIRTY)) {
        //    continue;
       // }

        const ID<Mesh> mesh_id = mesh_component.mesh.GetID();
        const ID<Material> material_id = mesh_component.material.GetID();
        const ID<Skeleton> skeleton_id = mesh_component.skeleton.GetID();

        if (mesh_component.material) {
            mesh_component.material->Update();
        }

        entity_draw_datas.PushBack(EntityDrawData {
            entity_id,
            mesh_id,
            material_id,
            skeleton_id,
            transform_component.transform.GetMatrix(),
            mesh_component.previous_model_matrix,
            bounding_box_component.world_aabb,
            mesh_component.material.IsValid() // @TODO: More efficent way of doing this
                ? uint32(mesh_component.material->GetBucket())
                : BUCKET_OPAQUE
        });

        if (mesh_component.previous_model_matrix != transform_component.transform.GetMatrix()) {
            // Update previous model matrix to current model matrix
            mesh_component.previous_model_matrix = transform_component.transform.GetMatrix();
            mesh_component.flags |= MESH_COMPONENT_FLAG_DIRTY;
        } else {
            mesh_component.flags &= ~MESH_COMPONENT_FLAG_DIRTY;
        }
    }

    if (entity_draw_datas.Any()) {
        PUSH_RENDER_COMMAND(
            UpdateEntityDrawDatas,
            std::move(entity_draw_datas)
        );
    }
}

} // namespace hyperion::v2