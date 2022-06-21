#ifndef HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H
#define HYPERION_V2_TERRAIN_PAGING_CONTROLLER_H

#include <scene/controllers/paging_controller.h>
#include <rendering/material.h>

namespace hyperion::v2 {

class TerrainPagingController : public PagingController {
public:
    TerrainPagingController(Extent3D patch_size, const Vector3 &scale);
    virtual ~TerrainPagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch) override;
    virtual void OnPatchRemoved(Patch *patch) override;

    virtual std::unique_ptr<Spatial> CreateTerrainChunk(const PatchInfo &patch_info);

private:
    Ref<Material> m_material;
};

} // namespace hyperion::v2

#endif