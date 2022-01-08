#include "ortho_camera.h"
#include "../../math/matrix_util.h"

namespace apex {
OrthoCamera::OrthoCamera(float left, float right, float bottom, float top, float near, float far)
    : Camera(512, 512, near, far),
      m_left(left),
      m_right(right),
      m_bottom(bottom),
      m_top(top)
{
}

void OrthoCamera::UpdateLogic(double dt)
{
}

void OrthoCamera::UpdateMatrices()
{
    MatrixUtil::ToLookAt(m_view_mat, m_translation, GetTarget(), m_up);
    MatrixUtil::ToOrtho(m_proj_mat,
        m_left,
        m_right,
        m_bottom,
        m_top,
        m_near,
        m_far
    );
}
}