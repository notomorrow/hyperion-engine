#include "terrain_paging_controller.h"
#include "../terrain_mesh_builder.h"
#include <builders/mesh_builder.h>
#include <engine.h>

namespace hyperion::v2 {

TerrainPagingController::TerrainPagingController(Extent3D patch_size, const Vector3 &scale)
    : PagingController("TerrainPagingController", patch_size, scale)
{
}

void TerrainPagingController::OnAdded()
{
    m_material = GetEngine()->resources.materials.Add(std::make_unique<Material>(
        "terrain_material"
    ));

    m_material.Init();

    PagingController::OnAdded();
}

void TerrainPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void TerrainPagingController::OnUpdate(GameCounter::TickUnit delta)
{
    PagingController::OnUpdate(delta);
}

void TerrainPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch added %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    if (patch->spatial == nullptr) {
        patch->spatial = GetEngine()->resources.spatials.Add(CreateTerrainChunk(patch->info));

        if (auto *scene = GetOwner()->GetScene()) {
            DebugLog(LogType::Debug, "Add terrain spatial with id #%u\n", patch->spatial->GetId().value);
            scene->AddSpatial(patch->spatial.IncRef());
        }
    }
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    if (patch->spatial != nullptr) {
        if (auto *scene = GetOwner()->GetScene()) {
            DebugLog(LogType::Debug, "Remove terrain spatial with id #%u\n", patch->spatial->GetId().value);
            scene->RemoveSpatial(patch->spatial);
        }

        patch->spatial = nullptr;
    }
}

std::unique_ptr<Spatial> TerrainPagingController::CreateTerrainChunk(const PatchInfo &patch_info)
{
    TerrainMeshBuilder builder(patch_info);
    builder.GenerateHeights(12345);

    // DebugLog(LogType::Debug, "Create patch at x: %f, y: %f\n", patch_info.coord.x, patch_info.coord.y);

    auto &shader = GetEngine()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
    auto mesh = GetEngine()->resources.meshes.Add(builder.BuildMesh());
    auto vertex_attributes = mesh->GetVertexAttributes();

    auto spatial = std::make_unique<Spatial>(
        std::move(mesh),
        shader.IncRef(),
        m_material.IncRef(),
        RenderableAttributeSet {
            .bucket            = Bucket::BUCKET_OPAQUE,
            .shader_id         = shader->GetId(),
            .vertex_attributes = vertex_attributes
        }
    );

    spatial->SetTranslation(Vector3(
        (patch_info.coord.x - 0.5f) * (patch_info.extent.ToVector3().Avg() - 1) * m_scale.x,
        5,
        (patch_info.coord.y - 0.5f) * (patch_info.extent.ToVector3().Avg() - 1) * m_scale.z
    ));

    return spatial;
}

} // namespace hyperion::v2