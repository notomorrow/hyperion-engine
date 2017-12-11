#include "perspective_camera.h"
#include "../../math/matrix_util.h"

namespace apex {
PerspectiveCamera::PerspectiveCamera(float fov, int width, int height, float near_clip, float far_clip)
    : Camera(width, height, near_clip, far_clip),
      fov(fov)
{
}

void PerspectiveCamera::UpdateLogic(double dt)
{
}

void PerspectiveCamera::UpdateMatrices()
{
    Vector3 target = translation + direction;

    MatrixUtil::ToLookAt(view_mat, translation, target, up);
    MatrixUtil::ToPerspective(proj_mat, fov, width, height, near_clip, far_clip);

    view_proj_mat = view_mat * proj_mat;
}
}