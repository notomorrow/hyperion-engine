#ifndef HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H
#define HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H

#include "../TerrainMeshBuilder.hpp"
#include <core/lib/Queue.hpp>
#include <scene/controllers/PagingController.hpp>
#include <rendering/Material.hpp>
#include <util/NoiseFactory.hpp>

#include <mutex>
#include <atomic>

namespace hyperion::v2 {

class TerrainPagingController : public PagingController {
public:
    TerrainPagingController(
        Seed seed,
        Extent3D patch_size,
        const Vector3 &scale,
        Float max_distance
    );
    virtual ~TerrainPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

    void AddEnqueuedChunks();

    NoiseCombinator m_noise_combinator;

private:
    struct TerrainGenerationResult {
        PatchInfo             patch_info;
        std::unique_ptr<Mesh> mesh;
    };

    Seed                                m_seed;

    UInt m_update_log_timer = 0;

    Ref<Material>                       m_material;

    std::mutex                          m_terrain_generation_mutex;
    std::atomic_bool                    m_terrain_generation_flag;

    // don't touch without mutex
    Queue<TerrainGenerationResult>      m_shared_terrain_mesh_queue;
    Queue<TerrainGenerationResult>      m_owned_terrain_mesh_queue;
};

} // namespace hyperion::v2

#endif