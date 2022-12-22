#include "BasicPagingController.hpp"

namespace hyperion::v2 {

BasicPagingController::BasicPagingController()
    : PagingController(Extent3D { 64, 64, 64 }, Vector3::one, 3.0f)
{
}

BasicPagingController::BasicPagingController(
    Extent3D patch_size,
    const Vector3 &scale,
    Float max_distance
) : PagingController(patch_size, scale, max_distance)
{
}

void BasicPagingController::OnAdded()
{
    PagingController::OnAdded();
}

void BasicPagingController::OnRemoved()
{
    PagingController::OnRemoved();
}

void BasicPagingController::OnPatchAdded(Patch *patch)
{
    DebugLog(LogType::Info, "Patch added %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}

void BasicPagingController::OnPatchRemoved(Patch *patch)
{
    DebugLog(LogType::Info, "Patch removed %f, %f\n", patch->info.coord.x, patch->info.coord.y);
}

} // namespace hyperion::v2