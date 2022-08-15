#include "TerrainPagingController.hpp"
#include <rendering/Texture.hpp>
#include <builders/MeshBuilder.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

TerrainPagingController::TerrainPagingController(
    Seed seed,
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : PagingController("TerrainPagingController", patch_size, scale, max_distance),
    m_noise_combinator(seed),
    m_seed(seed)
{
}

void TerrainPagingController::OnAdded()
{
    constexpr Float base_height = 20.0f;
    constexpr Float mountain_height = 150.0f;
    constexpr Float global_terrain_noise_scale = 1.0f;

    m_noise_combinator
        .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, mountain_height, 0.0f, Vector(1.0f, 1.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, base_height, 0.0f, Vector(100.0f, 100.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, base_height * 0.5f, 0.0f, Vector(50.0f, 50.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, base_height * 0.25f, 0.0f, Vector(25.0f, 25.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, base_height * 0.125f, 0.0f, Vector(12.5f, 12.5f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, base_height * 0.06f, 0.0f, Vector(6.25f, 6.25f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, base_height * 0.03f, 0.0f, Vector(3.125f, 3.125f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, base_height * 0.015f, 0.0f, Vector(1.56f, 1.56f, 0.0f, 0.0f) * global_terrain_noise_scale);

    m_material = GetEngine()->resources.materials.Add(new Material(
        "terrain_material"
    ));

    // m_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.2f, 0.99f, 0.5f, 1.0f));
    m_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.75f);
    m_material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
    // m_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 50.0f);
    // auto albedo_texture = GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_albedo.png").release());
    auto albedo_texture = GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/snow/snowdrift1_albedo.png").release());
    albedo_texture->GetImage().SetIsSRGB(true);
    m_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(albedo_texture));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/snow/snowdrift1_Normal-ogl.png").release()));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/snow/snowdrift1_Roughness.png").release()));
    // m_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/patchy-meadow1-ue/patchy-meadow1_normal-dx.png").release()));


    // m_material->SetTexture(Material::MATERIAL_TEXTURE_AO_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-ao.png")));
    // m_material->SetTexture(Material::MATERIAL_TEXTURE_PARALLAX_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Height.png")));
    // m_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1_Roughness.png")));
    // m_material->SetTexture(Material::MATERIAL_TEXTURE_METALNESS_MAP, GetEngine()->resources.textures.Add(GetEngine()->assets.Load<Texture>("textures/rocky_dirt1-ue/rocky_dirt1-metallic.png")));

    m_material.Init();

    PagingController::OnAdded();
}

void TerrainPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void TerrainPagingController::OnUpdate(GameCounter::TickUnit delta)
{
    if (m_terrain_generation_flag.load()) {
        AddEnqueuedChunks();
    }

    PagingController::OnUpdate(delta);

    ++m_update_log_timer;

    if (m_update_log_timer >= 1000) {
        DebugLog(
            LogType::Debug,
            "Currently have %u terrain chunks\n",
            static_cast<UInt>(m_patches.Size())
        );
        
        m_update_log_timer = 0;
    }
}

void TerrainPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch added at [%f, %f], enqueuing terrain generation\n", patch->info.coord.x, patch->info.coord.y);
    
    GetEngine()->terrain_thread.ScheduleTask([this, patch_info = patch->info]() {
        TerrainMeshBuilder builder(patch_info);
        builder.GenerateHeights(m_noise_combinator);

        auto mesh = builder.BuildMesh();

        std::lock_guard guard(m_terrain_generation_mutex);

        m_shared_terrain_mesh_queue.Push(TerrainGenerationResult {
            .patch_info = patch_info,
            .mesh = std::move(mesh)
        });

        m_terrain_generation_flag.store(true);
    });
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    if (patch->entity != nullptr) {
        if (auto *scene = GetOwner()->GetScene()) {
            DebugLog(LogType::Debug, "Remove terrain Entity with id #%u\n", patch->entity->GetId().value);

            scene->RemoveEntity(patch->entity);
        }

        patch->entity.Reset();
    }
}

void TerrainPagingController::AddEnqueuedChunks()
{
    m_terrain_generation_mutex.lock();
    m_owned_terrain_mesh_queue  = std::move(m_shared_terrain_mesh_queue);
    m_terrain_generation_mutex.unlock();

    size_t num_chunks_added = 0;

    while (m_owned_terrain_mesh_queue.Any()) {
        auto terrain_generation_result = m_owned_terrain_mesh_queue.Pop();
        const auto &patch_info = terrain_generation_result.patch_info;

        auto mesh = GetEngine()->resources.meshes.Add(terrain_generation_result.mesh.release());
        AssertThrow(mesh != nullptr);

        auto vertex_attributes = mesh->GetVertexAttributes();

        auto &shader = GetEngine()->shader_manager.GetShader(ShaderManager::Key::TERRAIN);
        
        auto entity = std::make_unique<Entity>(
            std::move(mesh),
            shader.IncRef(),
            m_material.IncRef(),
            RenderableAttributeSet {
                .bucket            = Bucket::BUCKET_OPAQUE,
                .shader_id         = shader->GetId(),
                .vertex_attributes = vertex_attributes
            }
        );

        entity->SetTranslation({
            (patch_info.coord.x - 0.5f) * (Vector(patch_info.extent).Avg() - 1) * m_scale.x,
            GetOwner()->GetTranslation().y,
            (patch_info.coord.y - 0.5f) * (Vector(patch_info.extent).Avg() - 1) * m_scale.z
        });

        if (auto *patch = GetPatch(patch_info.coord)) {
            if (patch->entity == nullptr) {
                patch->entity = GetEngine()->resources.entities.Add(entity.release());

                ++num_chunks_added;

                if (auto *scene = GetOwner()->GetScene()) {
                    DebugLog(LogType::Debug, "Add terrain Entity with id #%u\n", patch->entity->GetId().value);
                    scene->AddEntity(patch->entity.IncRef());
                }
            } else {
                DebugLog(
                    LogType::Warn,
                    "Patch at [%f, %f] already had an entity assigned!\n",
                    patch_info.coord.x, patch_info.coord.y
                );
            }
        } else {
            DebugLog(
                LogType::Warn,
                "Patch at [%f, %f] does not exist after generation completeted!\n",
                patch_info.coord.x, patch_info.coord.y
            );
        }
    }

    DebugLog(LogType::Debug, "Added %llu chunks\n", num_chunks_added);

    m_terrain_generation_flag.store(false);
}

} // namespace hyperion::v2