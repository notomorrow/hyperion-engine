#include "TerrainPagingController.hpp"
#include <rendering/Texture.hpp>
#include <util/MeshBuilder.hpp>
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
    constexpr Float mountain_height = 350.0f;
    constexpr Float global_terrain_noise_scale = 1.0f;

    m_noise_combinator
        .Use<WorleyNoiseGenerator>(0, NoiseCombinator::Mode::ADDITIVE, mountain_height, 0.0f, Vector(0.35f, 0.35f, 0.0f, 0.0f) * global_terrain_noise_scale)
        // .Use<SimplexNoiseGenerator>(1, NoiseCombinator::Mode::MULTIPLICATIVE, 0.5f, 0.5f, Vector(50.0f, 50.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(2, NoiseCombinator::Mode::ADDITIVE, base_height, 0.0f, Vector(100.0f, 100.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(3, NoiseCombinator::Mode::ADDITIVE, base_height * 0.5f, 0.0f, Vector(50.0f, 50.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(4, NoiseCombinator::Mode::ADDITIVE, base_height * 0.25f, 0.0f, Vector(25.0f, 25.0f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(5, NoiseCombinator::Mode::ADDITIVE, base_height * 0.125f, 0.0f, Vector(12.5f, 12.5f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(6, NoiseCombinator::Mode::ADDITIVE, base_height * 0.06f, 0.0f, Vector(6.25f, 6.25f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(7, NoiseCombinator::Mode::ADDITIVE, base_height * 0.03f, 0.0f, Vector(3.125f, 3.125f, 0.0f, 0.0f) * global_terrain_noise_scale)
        .Use<SimplexNoiseGenerator>(8, NoiseCombinator::Mode::ADDITIVE, base_height * 0.015f, 0.0f, Vector(1.56f, 1.56f, 0.0f, 0.0f) * global_terrain_noise_scale);

    m_material = CreateObject<Material>("terrain_material");

    // m_material->SetParameter(Material::MATERIAL_KEY_ALBEDO, Vector4(0.2f, 0.99f, 0.5f, 1.0f));
    m_material->SetParameter(Material::MATERIAL_KEY_ROUGHNESS, 0.5f);
    m_material->SetParameter(Material::MATERIAL_KEY_METALNESS, 0.0f);
    // m_material->SetParameter(Material::MATERIAL_KEY_UV_SCALE, 50.0f);

    if (auto albedo_texture = Engine::Get()->GetAssetManager().Load<Texture>("textures/snow/snowdrift1_albedo.png")) {
        albedo_texture->GetImage()->SetIsSRGB(true);
        m_material->SetTexture(Material::MATERIAL_TEXTURE_ALBEDO_MAP, std::move(albedo_texture));
    }

    m_material->SetTexture(Material::MATERIAL_TEXTURE_NORMAL_MAP, Engine::Get()->GetAssetManager().Load<Texture>("textures/snow/snowdrift1_Normal-ogl.png"));
    m_material->SetTexture(Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, Engine::Get()->GetAssetManager().Load<Texture>("textures/snow/snowdrift1_Roughness.png"));

    InitObject(m_material);

    PagingController::OnAdded();
}

void TerrainPagingController::OnRemoved()
{
    // remove all tasks
    for (const auto &enqueued_patch : m_enqueued_patches) {
        const auto &task = enqueued_patch.second;

        Engine::Get()->task_system.Unschedule(task);
    }

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
    // just in case a patch was quickly added, enqueued, removed, then added back again.
    if (m_enqueued_patches.Contains(patch->info.coord)) {
        DebugLog(
            LogType::Info,
            "Terrain patch at [%f, %f] already enqueued for generation, skipping.\n",
            patch->info.coord.x,
            patch->info.coord.y
        );

        return;
    }

    DebugLog(LogType::Info, "Terrain patch added at [%f, %f], enqueuing terrain generation\n", patch->info.coord.x, patch->info.coord.y);
    
    auto &shader = Engine::Get()->shader_manager.GetShader(ShaderManager::Key::TERRAIN);
    AssertThrow(shader.IsValid());

    const auto vertex_attributes = renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes;

    patch->entity = CreateObject<Entity>(
        Handle<Mesh>(), // mesh added later, after task thread generates it
        Handle<Shader>(shader),
        Handle<Material>(m_material),
        RenderableAttributeSet(
            MeshAttributes {
                .vertex_attributes = vertex_attributes
            },
            MaterialAttributes {
                .bucket = Bucket::BUCKET_OPAQUE
            },
            shader->GetID()
        )
    );

    patch->entity->SetName(String("terrain_chunk_")
        + String::ToString(static_cast<Int>(patch->info.coord.x))
        + "_" + String::ToString(static_cast<Int>(patch->info.coord.y)));

    patch->entity->SetTranslation({
        (patch->info.coord.x - 0.5f) * (Vector(patch->info.extent).Max() - 1) * m_scale.x,
        GetOwner()->GetTranslation().y,
        (patch->info.coord.y - 0.5f) * (Vector(patch->info.extent).Max() - 1) * m_scale.z
    });

    for (const ID<Scene> &id : GetOwner()->GetScenes()) {
        if (auto scene = Handle<Scene>(id)) {
            scene->AddEntity(Handle<Entity>(patch->entity));
        }
    }

    const auto task_ref = Engine::Get()->task_system.ScheduleTask([this, patch_info = patch->info]() {
        TerrainMeshBuilder builder(patch_info);
        builder.GenerateHeights(m_noise_combinator);

        auto mesh = builder.BuildMesh();

        m_terrain_generation_sp.Wait();

        m_shared_terrain_mesh_queue.Push(TerrainGenerationResult {
            .patch_info = patch_info,
            .mesh = std::move(mesh)
        });

        m_terrain_generation_sp.Signal();

        m_terrain_generation_flag.store(true);
    });

    m_enqueued_patches.Insert(patch->info.coord, task_ref);
}

void TerrainPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Terrain patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);

    const auto it = m_enqueued_patches.Find(patch->info.coord);

    if (it != m_enqueued_patches.End()) {
        DebugLog(
            LogType::Debug,
            "Unschedule task to generate terrain patch at coord [%f, %f]\n",
            it->first.x,
            it->first.y
        );

        Engine::Get()->task_system.Unschedule(it->second);

        m_enqueued_patches.Erase(it);
    }

    if (patch->entity == nullptr) {
        DebugLog(
            LogType::Warn,
            "Terrain patch has no entity attached!\n"
        );

        return;
    }

    for (const ID<Scene> &id : GetOwner()->GetScenes()) {
        if (auto scene = Handle<Scene>(id)) {
            DebugLog(LogType::Debug, "Remove terrain Entity with id #%u\n", patch->entity->GetID().value);

            if (!scene->RemoveEntity(patch->entity)) {
                DebugLog(
                    LogType::Warn,
                    "Terrain entity with id #%u not in Scene! Could cause mem leak if cannot from entities from scene.\n",
                    patch->entity->GetID().value
                );
            }
        }
    }

    patch->entity.Reset();
}

void TerrainPagingController::AddEnqueuedChunks()
{
    m_terrain_generation_sp.Wait();
    m_owned_terrain_mesh_queue = std::move(m_shared_terrain_mesh_queue);
    m_terrain_generation_sp.Signal();

    UInt num_chunks_added = 0u;

    while (m_owned_terrain_mesh_queue.Any()) {
        auto terrain_generation_result = m_owned_terrain_mesh_queue.Pop();
        const auto &patch_info = terrain_generation_result.patch_info;

        const auto enqueued_patches_it = m_enqueued_patches.Find(patch_info.coord);

        if (enqueued_patches_it == m_enqueued_patches.End()) {
            DebugLog(
                LogType::Info,
                "Terrain mesh at coord [%f, %f] no longer in map, must have been removed. Skipping.\n",
                patch_info.coord.x,
                patch_info.coord.y
            );

            continue;
        }

        m_enqueued_patches.Erase(enqueued_patches_it);

        DebugLog(
            LogType::Debug,
            "Add completed terrain mesh at coord [%f, %f]\n",
            patch_info.coord.x,
            patch_info.coord.y
        );

        auto mesh = std::move(terrain_generation_result.mesh);
        AssertThrow(mesh.IsValid());

        if (auto *patch = GetPatch(patch_info.coord)) {
            AssertThrow(patch->entity.IsValid());
            AssertThrow(!patch->entity->GetMesh().IsValid());

            ++num_chunks_added;

            patch->entity->SetMesh(std::move(mesh));
        } else {
            DebugLog(
                LogType::Warn,
                "Patch at [%f, %f] does not exist after generation completeted!\n",
                patch_info.coord.x, patch_info.coord.y
            );
        }
    }

    DebugLog(LogType::Debug, "Added %u chunks\n", num_chunks_added);

    m_terrain_generation_flag.store(false);
}

} // namespace hyperion::v2