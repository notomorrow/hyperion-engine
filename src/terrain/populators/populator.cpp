#include "populator.h"

#include "../../rendering/camera/camera.h"
#include "../../rendering/shader_manager.h"
#include "../../rendering/shaders/lighting_shader.h"
#include "../../asset/asset_manager.h"
#include "../terrain_chunk.h"
#include "../../util/mesh_factory.h"
#include "../../math/matrix_util.h"

#include <cmath>

namespace hyperion {
Populator::Populator(
    Camera *camera,
    unsigned long seed,
    double probability_factor,
    float tolerance,
    float max_distance,
    float spread,
    int num_entities_per_chunk,
    int num_patches,
    int patch_spread,
    bool use_batching
)
    : EntityControl(1.0), // 1 update / sec
      m_camera(camera),
      m_seed(seed),
      m_probability_factor(probability_factor),
      m_tolerance(tolerance),
      m_max_distance(max_distance),
      m_spread(spread),
      m_num_entities_per_chunk(num_entities_per_chunk),
      m_num_patches(num_patches),
      m_patch_spread(patch_spread),
      m_use_batching(use_batching),
      m_entity(new Entity("Populator node"))
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise(seed, &m_simplex_noise.octaves[i]);
        m_simplex_noise.frequencies[i] = pow(2.0, double(i));
        m_simplex_noise.amplitudes[i] = pow(0.5, OSN_OCTAVE_COUNT - i);
    }
}

Populator::~Populator()
{
    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        open_simplex_noise_free(m_simplex_noise.octaves[i]);
    }
}

// void Populator::Rebuild()
// {
//     m_patches.clear();
//     m_entity.reset(new Entity("Populator node"));
// }

void Populator::CreatePatches(const Vector2 &origin,
    const Vector2 &_center,
    float parent_size)
{

    parent->UpdateTransform(); // maybe not needed?

    float patch_size = parent_size / float(m_num_patches);

    for (int x = 0; x < m_num_patches; x++) {
        for (int z = 0; z < m_num_patches; z++) {
            Vector2 offset(x * patch_size, z * patch_size);
            Vector2 patch_location = origin + offset;

            Patch patch;
            patch.m_node = nullptr;
            patch.m_tile = GridTile(patch_location.x, patch_location.y, patch_size, patch_size, m_max_distance);
            patch.m_chunk_size = patch_size;
            patch.m_chunk_start = Vector3(patch_location.x, 0, patch_location.y);
            patch.m_num_entities_per_chunk = m_num_entities_per_chunk;

            patch.m_test_patch_color = Vector4(
                MathUtil::Random(0.0f, 1.0f),
                MathUtil::Random(0.0f, 1.0f),
                MathUtil::Random(0.0f, 1.0f),
                1.0f
            );
            m_patches.push_back(patch);
        }
    }
}

std::shared_ptr<Entity> Populator::CreateEntityNode(Patch &patch)
{
    auto node = std::make_shared<Entity>("Populator node");

    float placement = float(patch.m_chunk_size) / float(patch.m_num_entities_per_chunk);

    for (int x = 0; x < patch.m_num_entities_per_chunk; x++) {
        for (int z = 0; z < patch.m_num_entities_per_chunk; z++) {
            Vector3 entity_offset(
                float(x) * placement + (placement * 0.5f) + MathUtil::Random(-m_spread, m_spread),
                0,
                float(z) * placement + (placement * 0.5f) + MathUtil::Random(-m_spread, m_spread)
            );

            const Vector3 position = patch.m_chunk_start + entity_offset;
            const Vector3 global_position = parent->GetGlobalTranslation() + position;

            double chance = GetNoise(Vector2(global_position.x, global_position.z)) * 0.5 + 0.5;

            if (chance > m_probability_factor) {
                continue;
            }

            entity_offset.y = GetHeight(global_position);

            if (std::isnan(entity_offset.y)) {
                continue;
            }

            Vector3 normal = GetNormal(global_position);

            Matrix4 lookat_mat;
            MatrixUtil::ToLookAt(lookat_mat, normal, Vector3(0, 1, 0));

            auto object_node = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/grass/grass2.obj");
            // auto object_node = AssetManager::GetInstance()->LoadFromFile<Entity>("res/models/cube.obj");
            // auto object_node = std::make_shared<Entity>("Populator object"); // TODO: virtual method
            object_node->SetLocalRotation(Quaternion(lookat_mat));
            object_node->SetLocalTranslation(entity_offset);
            object_node->UpdateTransform();

            node->AddChild(object_node);
        }
    }

    if (m_use_batching) {
        std::vector<RenderableMesh_t> meshes = MeshFactory::GatherMeshes(node.get());

        if (!meshes.empty()) {
            auto merged_mesh = MeshFactory::MergeMeshes(meshes);
            merged_mesh->SetShader(ShaderManager::GetInstance()->GetShader<LightingShader>(ShaderProperties())); // TODO

            node.reset(new Entity(std::string("Populator node (batched) ") + std::to_string(patch.m_chunk_start.x) + "," + std::to_string(patch.m_chunk_start.y)));
            node->SetRenderable(merged_mesh);
            node->SetMaterial(std::get<2>(meshes.front()));
        }
    }

    for (size_t i = 0; i < node->NumChildren(); i++) {
        auto child = node->GetChild(i);

        if (child == nullptr) {
            continue;
        }

        if (auto renderable = child->GetRenderable()) {
            renderable->SetRenderBucket(Renderable::RB_PARTICLE);
        }
    }

    node->SetLocalTranslation(patch.m_chunk_start);

    return node;
}

double Populator::GetNoise(const Vector2 &location) const
{
    double result = 0.0;

    for (int i = 0; i < OSN_OCTAVE_COUNT; i++) {
        result += open_simplex_noise2(m_simplex_noise.octaves[i], location.x / m_simplex_noise.frequencies[i], location.y / m_simplex_noise.frequencies[i]) * m_simplex_noise.amplitudes[i];
    }

    return result;
}

float Populator::GetHeight(const Vector3 &location) const
{
    // TODO: More optimal way of doing this.
    // overload the class maybe, to be specific for terrain?
    if (TerrainChunk *chunk = dynamic_cast<TerrainChunk*>(parent)) {
        return chunk->HeightAtWorld(location);
    }

    return NAN;
}

Vector3 Populator::GetNormal(const Vector3 &location) const
{
    // TODO: More optimal way of doing this.
    // overload the class maybe, to be specific for terrain?
    if (TerrainChunk *chunk = dynamic_cast<TerrainChunk*>(parent)) {
        return chunk->NormalAtWorld(location);
    }

    return Vector3(NAN);
}

void Populator::OnAdded()
{
    parent->AddChild(m_entity);
}

void Populator::OnRemoved()
{
    parent->RemoveChild(m_entity);
}

void Populator::OnFirstRun(double dt)
{
    if (TerrainChunk *chunk = dynamic_cast<TerrainChunk*>(parent)) {
        const float size = (chunk->m_chunk_info.m_width + chunk->m_chunk_info.m_length) / 2.0f;
        const float scale = (chunk->m_chunk_info.m_scale.x + chunk->m_chunk_info.m_scale.z) / 2.0f;

        CreatePatches(
            Vector2(),
            Vector2(),
            size * scale
        );
    } else {
        CreatePatches(
            Vector2(),
            Vector2(),
            8.0f
        );
    }
}

void Populator::OnUpdate(double dt)
{
    Vector2 camera_vec(
        m_camera->GetTranslation().x,
        m_camera->GetTranslation().z
    );

    for (size_t i = 0; i < m_patches.size(); i++) {
        Patch &patch = m_patches[i];

        if (patch.m_tile.InRange(m_camera->GetTranslation() - parent->GetGlobalTranslation())) {
            if (patch.m_page_state != Patch::PageState::PATCH_LOADED) {
                std::cout << "patch loaded " << patch.m_chunk_start << "\n";
                if (patch.m_node == nullptr) {
                    patch.m_node = CreateEntityNode(patch);
                }
                
                m_entity->AddChild(patch.m_node);

                patch.m_page_state = Patch::PageState::PATCH_LOADED;
            }
        } else {
            switch (patch.m_page_state) {
            case Patch::PageState::PATCH_LOADED:
                std::cout << "patch unloading " << patch.m_chunk_start << "\n";
                patch.m_page_state = Patch::PageState::PATCH_UNLOADING;
                break;
            case Patch::PageState::PATCH_UNLOADING:
                std::cout << "patch uploaded " << patch.m_chunk_start << "\n";
                m_entity->RemoveChild(patch.m_node);
                // patch.m_node = nullptr;
                patch.m_page_state = Patch::PageState::PATCH_UNLOADED;
                break;
            case Patch::PageState::PATCH_UNLOADED:
                break;
            }
        }
        // TODO: cache time update for patch
    }
}
} // namespace hyperion
