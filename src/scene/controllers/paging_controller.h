#ifndef HYPERION_V2_PAGING_CONTROLLER_H
#define HYPERION_V2_PAGING_CONTROLLER_H

#include "../controller.h"
#include <scene/spatial.h>

#include <rendering/backend/renderer_structs.h>
#include <core/lib/flat_set.h>

#include <math/vector2.h>
#include <math/vector3.h>

#include <types.h>

#include <array>

namespace hyperion::v2 {

using renderer::Extent3D;

using PatchCoord = Vector2;

struct PatchNeighbor {
    PatchCoord coord{};
    bool       is_queued{false};

    Vector2 GetCenter() const { return coord - 0.5f; }
};

using PatchNeighbors = std::array<PatchNeighbor, 8>;

enum class PageState {
    UNLOADED,
    UNLOADING,
    WAITING,
    LOADED
};

struct PatchInfo {
    Extent3D       extent;
    PatchCoord     coord{0.0f};
    Vector3        scale{1.0f};
    PageState      state{PageState::UNLOADED};
    float          unload_timer{0.0f};
    PatchNeighbors neighbors{};
};

struct Patch {
    PatchInfo    info;
    Ref<Spatial> spatial;

    Vector2 GetCenter() const { return info.coord - 0.5f; }
};

class PagingController : public Controller {
    static constexpr GameCounter::TickUnit update_max{1.0f};
    static constexpr GameCounter::TickUnit queue_max{1.0f};
    static constexpr GameCounter::TickUnit patch_unload_time{2.0f};
    static constexpr float                 max_distance{3.5f};

public:
    PagingController(const char *name, Extent3D patch_size, const Vector3 &scale);
    virtual ~PagingController() override = default;

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

protected:
    virtual void OnPatchAdded(Patch *patch)   = 0;
    virtual void OnPatchRemoved(Patch *patch) = 0;
    
    using PatchList = std::vector<std::unique_ptr<Patch>>;

    struct PatchUpdate {
        PatchCoord coord;
        PageState  new_state;
    };

    static PatchNeighbors GetNeighbors(const PatchCoord &coord);

    PatchList::iterator FindPatch(const PatchCoord &coord)
    {
        return std::find_if(
            m_patches.begin(),
            m_patches.end(),
            [&coord](const auto &item) {
                return item->info.coord == coord;
            }
        );
    }

    Patch *GetPatch(const PatchCoord &coord)
    {
        const auto it = FindPatch(coord);

        if (it == m_patches.end()) {
            return nullptr;
        }

        return it->get();
    }

    PatchCoord WorldSpaceToCoord(const Vector3 &position) const;
    bool InRange(const Patch *patch, const PatchCoord &camera_coord) const;
    bool InRange(const PatchCoord &patch_center, const PatchCoord &camera_coord) const;

    virtual std::unique_ptr<Patch> CreatePatch(const PatchInfo &info);
    void AddPatch(const PatchCoord &coord);
    void RemovePatch(const PatchCoord &coord);
    void EnqueuePatch(const PatchCoord &coord);
    void PushUpdate(const PatchUpdate &update);
    void InitPatch(Patch *patch);

    PatchList               m_patches;
    std::queue<PatchUpdate> m_queue;
    FlatSet<PatchCoord>     m_queued_neighbors; // neighbors queued for addition, so we don't add duplicates
    Extent3D                m_patch_size;
    Vector3                 m_scale;
    GameCounter::TickUnit   m_update_timer;
    GameCounter::TickUnit   m_queue_timer;
};

} // namespace hyperion::v2

#endif
