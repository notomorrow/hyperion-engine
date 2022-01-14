#include "populator.h"

#include "../../rendering/camera/camera.h"
#include "../terrain_chunk.h"

namespace apex {
Populator::Populator(
    Camera *camera,
    float tolerance,
    float max_distance,
    float spread,
    int num_patches,
    int patch_spread,
    bool use_batching
)
    : m_camera(camera),
      m_tolerance(tolerance),
      m_max_distance(max_distance),
      m_spread(spread),
      m_num_patches(num_patches),
      m_patch_spread(patch_spread),
      m_use_batching(use_batching),
      m_entity(new Entity("Populator node"))
{
}

// void Populator::Rebuild()
// {
//     m_patches.clear();
//     m_entity.reset(new Entity("Populator node"));
// }

void Populator::CreatePatches(const Vector2 &origin,
    const Vector2 &center,
    int num_chunks,
    int num_entity_per_chunk,
    float parent_size)
{
    float chunk_size = parent_size / float(num_chunks);

    Vector2 max(parent_size / 2.0f);

    for (int x = 0; x < num_chunks; x++) {
        for (int z = 0; z < num_chunks; z++) {
            Vector2 offset(x * chunk_size, z * chunk_size);
            Vector2 chunk_loc = (origin + offset) - max;

            Patch patch;
            patch.m_tile = GridTile(chunk_loc.x, chunk_loc.y, chunk_size, chunk_size, 50.0f);
            patch.m_chunk_size = chunk_size;
            patch.m_chunk_start = Vector3(chunk_loc.x, 0.0f, chunk_loc.y);
            m_patches.push_back(patch);
        }
    }
}

void Populator::OnAdded()
{
}

void Populator::OnRemoved()
{    
}

void Populator::OnUpdate(double dt)
{
    Vector2 camera_vec(
        m_camera->GetTranslation().x,
        m_camera->GetTranslation().z
    );

    for (auto &patch : m_patches) {
        // if (patch.m_tile.InRange(camera_vec - ))
    }
}
} // namespace apex
