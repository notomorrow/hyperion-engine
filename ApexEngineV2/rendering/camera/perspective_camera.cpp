#include "perspective_camera.h"
#include "../../math/matrix_util.h"

namespace hyperion {
PerspectiveCamera::PerspectiveCamera(float fov, int width, int height, float near, float far)
    : Camera(width, height, near, far)
{
    m_fov = fov;
}

void PerspectiveCamera::UpdateLogic(double dt)
{
}

void PerspectiveCamera::UpdateMatrices()
{
    MatrixUtil::ToLookAt(m_view_mat, m_translation, GetTarget(), m_up);
    MatrixUtil::ToPerspective(m_proj_mat, m_fov, m_width, m_height, m_near, m_far);
}
} // namespace hyperion
