#include "paging_controller.h"

#include "scene/node.h"
#include "scene/scene.h"

namespace hyperion::v2 {

auto PagingController::GetNeighbors(const PatchCoord &coord) -> PatchNeighbors
{
    return{
        PatchNeighbor{coord + PatchCoord{1, 0}},
        PatchNeighbor{coord + PatchCoord{-1, 0}},
        PatchNeighbor{coord + PatchCoord{0, 1}},
        PatchNeighbor{coord + PatchCoord{0, -1}},
        PatchNeighbor{coord + PatchCoord{1, -1}},
        PatchNeighbor{coord + PatchCoord{-1, -1}},
        PatchNeighbor{coord + PatchCoord{1, 1}},
        PatchNeighbor{coord + PatchCoord{-1, 1}}
    };
}

PagingController::PagingController(const char *name, Extent3D patch_size, const Vector3 &scale)
    : Controller(name),
      m_patch_size(patch_size),
      m_scale(scale),
      m_update_timer(0.0f),
      m_queue_timer(0.0f)
{
}

void PagingController::OnAdded()
{
    AddPatch(PatchCoord{0, 0});
}

void PagingController::OnRemoved()
{
    std::vector<PatchCoord> patch_coords(m_patches.size());

    for (size_t i = 0; i < m_patches.size(); i++) {
        patch_coords[i] = m_patches[i]->info.coord;
    }

    for (const auto &coord : patch_coords) {
        RemovePatch(coord);
    }
}

void PagingController::OnUpdate(GameCounter::TickUnit delta)
{
    auto *node = GetOwner()->GetParent();

    if (node == nullptr) {
        DebugLog(
            LogType::Warn,
            "Entity #%u not attached to any node, cannot get scene camera.\n",
            GetOwner()->GetId().value
        );

        return;
    }

    Scene *scene = node->GetScene();

    if (scene == nullptr) {
        DebugLog(
            LogType::Warn,
            "PagingController on Node not attached to scene\n"
        );

        return;
    }

    auto *camera = scene->GetCamera();

    if (camera == nullptr) {
        DebugLog(
            LogType::Warn,
            "PagingController on Scene with no Camera\n"
        );

        return;
    }

    const auto camera_coord = WorldSpaceToCoord(camera->GetTranslation());

    // ensure a patch right under the camera exists
    // if all patches are removed we are not able to add any other linked ones otherwise
    if (GetPatch(camera_coord) == nullptr) {
        EnqueuePatch(camera_coord);
    }

    m_update_timer += delta;
    m_queue_timer  += delta;

    if (m_queue_timer >= queue_max) {
        while (!m_queue.empty()) {
            const auto &top = m_queue.front();
            Patch *patch = nullptr;

            switch (top.new_state) {
            case PageState::WAITING:
                AddPatch(top.coord);

                break;
            case PageState::UNLOADED:
                RemovePatch(top.coord);
                break;
            default:
                patch = GetPatch(top.coord);

                if (patch == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "Patch at %f, %f was not found when updating state\n",
                        top.coord.x, top.coord.y
                    );

                    break;
                }

                patch->info.state = top.new_state;

                break;
            }

            m_queue.pop();
        }

        m_queue_timer = 0.0f;
    }

    if (m_update_timer >= update_max) {
        for (auto &patch : m_patches) {
            AssertThrow(patch != nullptr);

            switch (patch->info.state) {
            case PageState::LOADED:
                patch->info.unload_timer = 0.0f;

                if (InRange(patch.get(), camera_coord)) {
                    for (auto &neighbor : patch->info.neighbors) {
                        if (GetPatch(neighbor.coord) == nullptr && InRange(neighbor.GetCenter(), camera_coord)) {
                            EnqueuePatch(neighbor.coord);
                        }
                    }
                } else {
                    PushUpdate({
                        .coord     = patch->info.coord,
                        .new_state = PageState::UNLOADING
                    });
                }

                break;

            case PageState::UNLOADING:
                if (InRange(patch.get(), camera_coord)) {
                    PushUpdate({
                        .coord     = patch->info.coord,
                        .new_state = PageState::LOADED
                    });
                } else {
                    patch->info.unload_timer += m_update_timer;

                    if (patch->info.unload_timer >= patch_unload_time) {
                        PushUpdate({
                            .coord     = patch->info.coord,
                            .new_state = PageState::UNLOADED
                        });
                    }
                }

                break;
            }
        }

        m_update_timer = 0.0f;
    }
}

auto PagingController::WorldSpaceToCoord(const Vector3 &position) const -> PatchCoord
{
    Vector3 scaled = position - GetOwner()->GetTranslation();
    scaled *= Vector3::One() / (m_scale * (m_patch_size.ToVector3() - 1.0f));
    scaled = MathUtil::Floor(scaled);

    return PatchCoord{scaled.x, scaled.z};
}

bool PagingController::InRange(const Patch *patch, const PatchCoord &camera_coord) const
{
    return InRange(patch->GetCenter(), camera_coord);
}

bool PagingController::InRange(const PatchCoord &patch_center, const PatchCoord &camera_coord) const
{
    return camera_coord.Distance(patch_center) <= max_distance;
}

auto PagingController::CreatePatch(const PatchInfo &info) -> std::unique_ptr<Patch>
{
    return std::make_unique<Patch>(Patch{
        .info = info
    });
}

void PagingController::AddPatch(const PatchCoord &coord)
{
    const PatchInfo info{
        .extent    = m_patch_size,
        .coord     = coord,
        .scale     = m_scale,
        .state     = PageState::LOADED,
        .neighbors = GetNeighbors(coord)
    };

    auto patch = CreatePatch(info);
    InitPatch(patch.get());
    
    m_patches.push_back(std::move(patch));

    const auto neighbor_it = m_queued_neighbors.Find(coord);

    if (neighbor_it != m_queued_neighbors.end()) {
        m_queued_neighbors.Erase(neighbor_it);
    }
}

void PagingController::RemovePatch(const PatchCoord &coord)
{
    const auto it = FindPatch(coord);

    AssertThrow(it != m_patches.end());

    OnPatchRemoved(it->get());

    m_patches.erase(it);

    const auto neighbor_it = m_queued_neighbors.Find(coord);

    if (neighbor_it != m_queued_neighbors.end()) {
        m_queued_neighbors.Erase(neighbor_it);
    }
}

void PagingController::EnqueuePatch(const PatchCoord &coord)
{
    if (m_queued_neighbors.Find(coord) == m_queued_neighbors.End()) {
        PushUpdate({
            .coord     = coord,
            .new_state = PageState::WAITING
        });

        m_queued_neighbors.Insert(coord);
    }
}

void PagingController::PushUpdate(const PatchUpdate &update)
{
    m_queue.push(update);
}

void PagingController::InitPatch(Patch *patch)
{
    AssertThrow(patch != nullptr);

    patch->info.state = PageState::LOADED;
    
    OnPatchAdded(patch);
}

} // namespace hyperion::v2