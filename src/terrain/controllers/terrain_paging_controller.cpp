#include "terrain_paging_controller.h"
#include "../terrain_mesh_builder.h"
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
        patch->spatial.Init();
        patch->spatial->Update(GetEngine(), 0.1f); //tmp

        // if (auto *node = GetOwner()->GetNode()) {
        //     auto *patch_node = node->AddChild();
        //     patch_node->SetSpatial(patch->spatial.IncRef());
        // }
    }
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    if (patch->spatial != nullptr) {
        // if (auto *node = patch->spatial->GetNode()) {
        //     if (!patch->spatial->GetNode()->Remove()) {
        //         DebugLog(LogType::Warn, "Failed to remove node\n");
        //     }
        // }

        patch->spatial = nullptr;
    }
}

std::unique_ptr<Spatial> TerrainPagingController::CreateTerrainChunk(const PatchInfo &patch_info)
{
    TerrainMeshBuilder builder(patch_info);
    builder.GenerateHeights(12345);

    auto &shader = GetEngine()->shader_manager.GetShader(ShaderManager::Key::BASIC_FORWARD);
    auto mesh = GetEngine()->resources.meshes.Add(builder.BuildMesh());

    auto spatial = std::make_unique<Spatial>(
        std::move(mesh),
        shader.IncRef(),
        m_material.IncRef(),
        RenderableAttributeSet {
            .bucket            = Bucket::BUCKET_OPAQUE,
            .shader_id         = shader->GetId(),
            .vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
        }
    );

    spatial->SetTranslation(Vector3(
        (patch_info.coord.x - 0.5f) * (patch_info.extent.ToVector3().Length() - 1) * m_scale.x,
        0,
        (patch_info.coord.y - 0.5f) * (patch_info.extent.ToVector3().Length() - 1) * m_scale.z
    ));

    return spatial;
}

} // namespace hyperion::v2